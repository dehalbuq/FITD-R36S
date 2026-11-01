package org.libsdl.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * Explicit user import of legitimately obtained DOS game data through SAF.
 * This APK never includes, downloads or distributes the commercial assets.
 * Game files are stored app-private and accessed by the native FITD process.
 */
public class FITDImportActivity extends Activity {
    private static final int REQUEST_GAME_FOLDER=412;
    private TextView status;
    private Button play;
    private Button importFiles;
    private volatile boolean importing=false;
    private int fileCount=0;
    private long byteCount=0;
    private static final String NOTICE =
        "FITD Android ARMv7 é um port comunitário independente do motor livre " +
        "Free in the Dark. Não contém o jogo original Alone in the Dark, " +
        "seus gráficos, sons ou arquivos comerciais.\n\n" +
        "Você precisa obter legalmente uma cópia compatível de Alone in the Dark " +
        "e selecionar a pasta dos arquivos originais no armazenamento do aparelho. " +
        "Não disponibilizamos downloads não autorizados.\n\n" +
        "Motor: yaz0r/FITD e colaboradores. Referências: jmimu/FITD e " +
        "AloneInTheDarkReHaunted. Port Android: github.com/dehalbuq/FITD-R36S.\n\n" +
        "Código aberto e arquivos comerciais possuem licenças diferentes. " +
        "Não há associação ou endosso dos titulares do jogo.";

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setTitle("FITD Android ARMv7");
        LinearLayout layout=new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int pad=(int)(18*getResources().getDisplayMetrics().density);
        layout.setPadding(pad,pad,pad,pad);
        ScrollView scroll=new ScrollView(this);
        scroll.addView(layout);
        TextView title=new TextView(this);
        title.setText("FITD\nANDROID ARMv7");
        title.setTextSize(26);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);
        TextView legal=new TextView(this);
        legal.setText(NOTICE);
        legal.setTextSize(15);
        legal.setPadding(0,pad,0,pad);
        layout.addView(legal);
        status=new TextView(this);
        status.setTextSize(16);
        layout.addView(status);
        importFiles=new Button(this);
        importFiles.setText("Importar minha pasta do jogo original");
        importFiles.setOnClickListener(v->selectFolder());
        layout.addView(importFiles);
        play=new Button(this);
        play.setText("Jogar com controle Bluetooth");
        play.setOnClickListener(v->{
            if(gameFilesPresent())
                startActivity(new Intent(this,SDLActivity.class));
            else
                showError("Selecione uma pasta que contenha LISTBOD2.PAK e os demais arquivos originais do jogo.");
        });
        layout.addView(play);
        TextView help=new TextView(this);
        help.setText("Controles: direcional ou analógico esquerdo para movimentar; " +
            "A para ação; START/B para menu ou voltar. Emparelhe seu controle " +
            "Bluetooth nas configurações do Android antes de jogar.\n\n" +
            "O APK precisa de OpenGL ES 3.0 e Android 5.0 ou superior. " +
            "Arquivos salvos ficam na pasta privativa do aplicativo. " +
            "Faça backup antes de desinstalar.");
        help.setTextSize(13);
        help.setPadding(0,pad,0,0);
        layout.addView(help);
        setContentView(scroll);
        refresh();
    }
    private boolean gameFilesPresent(){
        return new File(getFilesDir(),"LISTBOD2.PAK").isFile()
            && new File(getFilesDir(),"LISTBODY.PAK").isFile();
    }
    private void refresh(){
        play.setEnabled(gameFilesPresent()&&!importing);
        importFiles.setEnabled(!importing);
        if(!importing)status.setText(gameFilesPresent()
            ? "Arquivos originais encontrados. Você pode iniciar o motor."
            : "Nenhum jogo encontrado. Importe uma pasta obtida legalmente.");
    }
    private void selectFolder(){
        Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                      | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent,REQUEST_GAME_FOLDER);
    }
    @Override @SuppressWarnings("deprecation")
    protected void onActivityResult(int request,int result,Intent data){
        super.onActivityResult(request,result,data);
        if(request!=REQUEST_GAME_FOLDER||result!=RESULT_OK||
           data==null||data.getData()==null)return;
        final Uri uri=data.getData();
        importing=true;
        status.setText("Copiando seus arquivos originais. Não feche o aplicativo.");
        refresh();
        new Thread(()->{
            String message;
            try{
                fileCount=0;byteCount=0;
                final String docId=DocumentsContract.getTreeDocumentId(uri);
                importTree(uri,docId,0);
                message=fileCount+" arquivos copiados ("+
                    (byteCount/(1024*1024))+" MiB). "+
                    (gameFilesPresent()?"Pronto para jogar.":
                     "Não encontrei LISTBOD2.PAK/LISTBODY.PAK. Verifique a pasta.");
            }catch(Exception ex){
                message="Falha ao copiar: "+ex.getClass().getSimpleName()+
                    " — "+String.valueOf(ex.getMessage());
            }
            final String msg=message;
            runOnUiThread(()->{
                importing=false;
                refresh();
                status.setText(msg);
            });
        },"FITD-Import").start();
    }
    private void importTree(Uri tree,String parentId,int depth)throws Exception{
        if(depth>4)return;
        Uri children=DocumentsContract.buildChildDocumentsUriUsingTree(tree,parentId);
        String[] projection={
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try(Cursor c=getContentResolver().query(children,projection,null,null,null)){
            if(c==null)return;
            while(c.moveToNext()){
                if(fileCount>=160)throw new IllegalStateException("Limite de 160 arquivos excedido");
                String id=c.getString(0);
                String name=c.getString(1);
                String mime=c.getString(2);
                if(DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)){
                    importTree(tree,id,depth+1);
                    continue;
                }
                if(name==null||!name.matches("[A-Za-z0-9_.&-]{1,90}"))continue;
                String upper=name.toUpperCase(Locale.US);
                if(!(upper.endsWith(".PAK")||upper.endsWith(".ITD")||
                     upper.endsWith(".TAT")||upper.endsWith(".CC1")||
                     upper.endsWith(".BBS")||upper.endsWith(".COM")))continue;
                // Never overwrite saved games from a previous play session.
                File target=new File(getFilesDir(),upper);
                if(upper.matches("SAVE[0-9]+\\.ITD")&&target.isFile())continue;
                Uri doc=DocumentsContract.buildDocumentUriUsingTree(tree,id);
                try(InputStream in=getContentResolver().openInputStream(doc)){
                    if(in==null)continue;
                    try(OutputStream out=new FileOutputStream(target)){
                        byte[] buffer=new byte[65536];int read;
                        while((read=in.read(buffer))!=-1){
                            out.write(buffer,0,read);
                            byteCount+=read;
                        }
                    }
                }
                fileCount++;
            }
        }
    }
    private void showError(String message){
        new AlertDialog.Builder(this)
            .setTitle("Arquivos necessários").setMessage(message)
            .setPositiveButton("Entendi",null).show();
    }
}

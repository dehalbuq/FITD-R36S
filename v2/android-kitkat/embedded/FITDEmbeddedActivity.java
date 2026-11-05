package org.libsdl.app;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.*;
import java.security.MessageDigest;
import java.util.*;
import java.util.zip.*;

/**
 * First-run installer for an ARMv7, Android API19+ SDL2 engine-core test APK.
 * An APK with empty placeholder PAK files is compiled on public CI; any actual
 * proprietary game assets must be injected and re-signed privately/offline.
 * This activity prepares user-supplied game assets and starts the real FITD engine.
 */
public final class FITDEmbeddedActivity extends Activity {
    private static final String TAG = "FITD-EMBEDDED";
    private TextView status;
    private Button retry;
    private boolean working;

    private static final class Item {
        long length;
        String hash;
        Item(long length, String hash) { this.length=length; this.hash=hash; }
    }
    private static String hex(byte[] bytes) {
        StringBuilder s=new StringBuilder(bytes.length*2);
        for (byte b:bytes) {
            s.append(Character.forDigit((b>>4)&15,16));
            s.append(Character.forDigit(b&15,16));
        }
        return s.toString();
    }
    private static String hashFile(File file) throws Exception {
        MessageDigest digest=MessageDigest.getInstance("SHA-256");
        try(InputStream in=new FileInputStream(file)) {
            byte[] buf=new byte[32768]; int n;
            while((n=in.read(buf))!=-1) digest.update(buf,0,n);
        }
        return hex(digest.digest());
    }
    private static boolean safeName(String n) {
        return n!=null && n.matches("[A-Z0-9_.&-]{1,90}") &&
            !(n.startsWith("SAVE") && n.endsWith(".ITD")) &&
            (n.endsWith(".PAK")||n.endsWith(".ITD")||n.endsWith(".TAT")||
             n.endsWith(".CC1")||n.endsWith(".BBS"));
    }
    private Map<String,Item> manifest() throws Exception {
        Map<String,Item> result=new LinkedHashMap<String,Item>();
        try(BufferedReader in=new BufferedReader(new InputStreamReader(
                getAssets().open("fitd-manifest.txt"),"UTF-8"))) {
            String line;
            while((line=in.readLine())!=null) {
                String[] parts=line.split("\\|",-1);
                if(parts.length!=3 || !safeName(parts[0])) throw new IOException("Manifesto inválido");
                long length=Long.parseLong(parts[1]);
                if(length<1||length>33554432L||!parts[2].matches("[a-f0-9]{64}"))
                    throw new IOException("Arquivo incorreto: "+parts[0]);
                if(result.put(parts[0],new Item(length,parts[2]))!=null)
                    throw new IOException("Duplicado: "+parts[0]);
            }
        }
        if(result.size()<2 || result.size()>100 ||
           !result.containsKey("LISTBODY.PAK") ||
           !result.containsKey("LISTBOD2.PAK")) throw new IOException("Faltam PAK essenciais");
        return result;
    }
    private void show(final String message) {
        runOnUiThread(() -> { if(!isFinishing()) status.setText(message); });
    }
    private void install() throws Exception {
        Map<String,Item> items=manifest();
        Set<String> found=new HashSet<String>();
        long total=0;
        File dir=getFilesDir();
        if(!dir.isDirectory())throw new IOException("Armazenamento indisponível");
        try(ZipInputStream zip=new ZipInputStream(new BufferedInputStream(
                getAssets().open("fitd-data.zip")))) {
            ZipEntry entry;
            while((entry=zip.getNextEntry())!=null) {
                String name=entry.getName();
                Item expected=items.get(name);
                if(entry.isDirectory()||expected==null||!safeName(name)||!found.add(name))
                    throw new IOException("Entrada inesperada: "+name);
                total+=expected.length;
                if(total>134217728L)throw new IOException("Pacote grande demais");
                show("Conferindo recursos: "+found.size()+"/"+items.size()+"\\n"+name);
                File dest=new File(dir,name);
                if(dest.isFile()&&dest.length()==expected.length&&
                   hashFile(dest).equals(expected.hash)) {
                    zip.closeEntry();
                    continue;
                }
                File temp=new File(dir,name+".fitdtmp");
                if(temp.exists()&&!temp.delete())throw new IOException("Tmp bloqueado");
                MessageDigest digest=MessageDigest.getInstance("SHA-256");
                long written=0;
                try {
                    try(FileOutputStream out=new FileOutputStream(temp)) {
                        byte[] buf=new byte[32768];int n;
                        while((n=zip.read(buf))!=-1) {
                            written+=n;
                            if(written>expected.length)throw new IOException("Tamanho excedido: "+name);
                            digest.update(buf,0,n);
                            out.write(buf,0,n);
                        }
                        out.getFD().sync();
                    }
                    if(written!=expected.length||!hex(digest.digest()).equals(expected.hash))
                        throw new IOException("SHA-256 falhou: "+name);
                    if(dest.exists()&&!dest.delete())throw new IOException("Não consegui atualizar: "+name);
                    if(!temp.renameTo(dest))throw new IOException("Não consegui instalar: "+name);
                } finally {if(temp.exists())temp.delete();}
                zip.closeEntry();
            }
        }
        if(found.size()!=items.size())throw new IOException("ZIP incompleto");
        if(!new File(dir,"LISTBODY.PAK").isFile()||
           !new File(dir,"LISTBOD2.PAK").isFile())throw new IOException("PAK essencial ausente");
        Log.i(TAG,"PASS: "+found.size()+" game assets verified and installed to app files");
    }
    private synchronized void begin() {
        if(working)return;
        working=true;
        retry.setVisibility(View.GONE);
        new Thread(()->{
            try{
                install();
                runOnUiThread(()->{
                    if(isFinishing())return;
                    working=false;
                    status.setText("Dados incorporados verificados. Abrindo teste do motor...");
                    startActivity(new Intent(this,SDLActivity.class));
                    finish();
                });
            }catch(Exception ex){
                Log.e(TAG,"Cannot install assets",ex);
                runOnUiThread(()->{
                    working=false;
                    if(!isFinishing()){
                        status.setText("Falha na instalação:\\n"+ex.getMessage());
                        retry.setVisibility(View.VISIBLE);
                    }
                });
            }
        },"FITD-embedded-assets").start();
    }
    @Override public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout root=new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setGravity(Gravity.CENTER);
        root.setPadding(22,22,22,22);
        root.setBackgroundColor(Color.rgb(13,16,31));
        TextView title=new TextView(this);
        title.setText("ALONE IN THE DARK\\nFITD ARMv7 - TESTE 20");
        title.setTextColor(Color.WHITE);
        title.setTextSize(22);
        title.setGravity(Gravity.CENTER);
        root.addView(title);
        status=new TextView(this);
        status.setTextColor(Color.LTGRAY);
        status.setTextSize(16);
        status.setGravity(Gravity.CENTER);
        status.setPadding(0,20,0,20);
        root.addView(status);
        retry=new Button(this);
        retry.setText("TENTAR NOVAMENTE");
        retry.setOnClickListener(v->begin());
        root.addView(retry);
        setContentView(root);
        show("Preparando dados incorporados...");
        begin();
    }
}

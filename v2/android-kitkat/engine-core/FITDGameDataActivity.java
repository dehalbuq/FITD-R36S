package org.libsdl.app;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ClipData;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/**
 * FITD ARMv7 Game Data Test 05: import legally provided original game files.
 *
 * Android KitKat (API 19): ACTION_OPEN_DOCUMENT with multiple selections.
 * Lollipop+ (API 21+): ACTION_OPEN_DOCUMENT_TREE for an entire folder.
 *
 * Does NOT launch the playable FITD engine. The native screen runs the
 * original PAK reader and decompressor inside an app-private data directory.
 * No broad storage permissions, no external downloads, no bundled game files.
 */
public final class FITDGameDataActivity extends Activity {
    private static final int REQUEST_TREE = 351;
    private static final int REQUEST_FILES = 352;
    private static final int MAX_FILES = 350;
    private static final long MAX_SINGLE_FILE = 128L * 1024L * 1024L;
    private static final long MAX_TOTAL_BYTES = 768L * 1024L * 1024L;
    private int copiedFiles = 0;
    private long copiedBytes = 0;
    private boolean copying = false;
    private TextView status;
    private Button importButton;
    private Button startButton;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        setTitle("FITD ARMv7 - Teste 05");
        ScrollView scroll = new ScrollView(this);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int pad = Math.round(18 * getResources().getDisplayMetrics().density);
        layout.setPadding(pad, pad, pad, pad);
        scroll.addView(layout);

        TextView title = new TextView(this);
        title.setText("ALONE IN THE DARK\nMotor FITD - Teste 05");
        title.setTextSize(24);
        title.setGravity(Gravity.CENTER);
        layout.addView(title);

        TextView instructions = new TextView(this);
        instructions.setText(
            "Teste de leitura dos arquivos originais do jogo no Android ARMv7.\n" +
            "O APK contém código do motor FITD, mas ainda NÃO executa a partida.\n\n" +
            "Selecione uma pasta com os arquivos de sua instalação legítima de " +
            "Alone in the Dark. Os arquivos são copiados para o espaço privado " +
            "do aplicativo e não enviados pela internet.\n\n" +
            "Android 4.4: selecione vários arquivos manualmente. " +
            "Android 5.0 ou superior: selecione a pasta completa.\n\n" +
            "Créditos: yaz0r/FITD, jmimu/FITD e dehalbuq/FITD-R36S. " +
            "O jogo original pertence aos respectivos titulares; " +
            "nenhum arquivo comercial é incluído neste APK.");
        instructions.setTextSize(14);
        instructions.setPadding(0, pad, 0, pad);
        layout.addView(instructions);

        status = new TextView(this);
        status.setTextSize(15);
        status.setPadding(0, 0, 0, pad);
        layout.addView(status);

        importButton = new Button(this);
        importButton.setText(Build.VERSION.SDK_INT >= 21 ?
            "IMPORTAR PASTA DO JOGO" : "SELECIONAR ARQUIVOS DO JOGO");
        importButton.setOnClickListener(v -> selectGameFiles());
        layout.addView(importButton);

        startButton = new Button(this);
        startButton.setText("TESTAR LEITURA DO MOTOR");
        startButton.setOnClickListener(v -> {
            if (!hasRequiredFiles()) {
                new AlertDialog.Builder(this).setTitle("Arquivos ausentes")
                    .setMessage("Importe LISTBOD2.PAK e LISTBODY.PAK antes de executar o teste.")
                    .setPositiveButton("OK", null).show();
                return;
            }
            startActivity(new Intent(this, SDLActivity.class));
        });
        layout.addView(startButton);

        TextView note = new TextView(this);
        note.setText("O teste abre uma tela com triângulo e controles Bluetooth " +
            "enquanto verifica a leitura de PAK. Não espere entrar no jogo nesta etapa.\n" +
            "Os dados ficam em armazenamento privado. Desinstalar apaga esses dados.");
        note.setTextSize(12);
        note.setPadding(0, pad, 0, 0);
        layout.addView(note);

        setContentView(scroll);
        refresh();
    }

    private boolean hasRequiredFiles() {
        return new File(getFilesDir(), "LISTBOD2.PAK").isFile() &&
               new File(getFilesDir(), "LISTBODY.PAK").isFile();
    }

    private void refresh() {
        importButton.setEnabled(!copying);
        startButton.setEnabled(!copying && hasRequiredFiles());
        if (!copying) status.setText(hasRequiredFiles()
            ? "Arquivos básicos presentes. Pronto para testar a leitura."
            : "Dados do jogo ausentes. Importe a pasta/arquivos originais.");
    }

    private void selectGameFiles() {
        Intent intent;
        int request;
        if (Build.VERSION.SDK_INT >= 21) {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            request = REQUEST_TREE;
        } else {
            intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
            request = REQUEST_FILES;
        }
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivityForResult(intent, request);
        } catch (Exception e) {
            status.setText("Seletor de documentos indisponível: " + e.getMessage());
        }
    }

    @Override protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if ((request != REQUEST_TREE && request != REQUEST_FILES) ||
            result != RESULT_OK || data == null) return;
        copying = true;
        copiedFiles = 0;
        copiedBytes = 0;
        status.setText("Importando arquivos. Não feche o aplicativo...");
        importButton.setEnabled(false);
        startButton.setEnabled(false);
        final Intent picked = data;
        final int method = request;
        new Thread(() -> {
            String message;
            try {
                if (method == REQUEST_TREE) {
                    Uri tree = picked.getData();
                    if (tree == null) throw new IllegalArgumentException("Pasta inválida");
                    String rootId = DocumentsContract.getTreeDocumentId(tree);
                    copyTree(tree, rootId, 0);
                } else {
                    ClipData selections = picked.getClipData();
                    if (selections != null) {
                        for (int i = 0; i < selections.getItemCount(); i++) {
                            copyDocument(selections.getItemAt(i).getUri(), null);
                        }
                    } else {
                        copyDocument(picked.getData(), null);
                    }
                }
                message = "Importados " + copiedFiles + " arquivos (" +
                    (copiedBytes / (1024 * 1024)) + " MiB). " +
                    (hasRequiredFiles()
                    ? "Arquivos básicos presentes. Execute o teste."
                    : "LISTBOD2.PAK ou LISTBODY.PAK ainda ausente.");
            } catch (Exception e) {
                message = "Importação interrompida: " +
                    e.getClass().getSimpleName() + " - " + e.getMessage();
            }
            final String finalMessage = message;
            runOnUiThread(() -> {
                copying = false;
                refresh();
                status.setText(finalMessage);
            });
        }, "FITD-GameDataImport").start();
    }

    private void copyTree(Uri tree, String parentId, int depth) throws Exception {
        if (depth > 5) return;
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(tree, parentId);
        String[] projection = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        };
        try (Cursor cursor = getContentResolver().query(children, projection, null, null, null)) {
            if (cursor == null) throw new IllegalStateException("Provedor não permite listar a pasta");
            while (cursor.moveToNext()) {
                String id = cursor.getString(0);
                String name = cursor.getString(1);
                String type = cursor.getString(2);
                if (DocumentsContract.Document.MIME_TYPE_DIR.equals(type)) {
                    copyTree(tree, id, depth + 1);
                } else {
                    copyDocument(DocumentsContract.buildDocumentUriUsingTree(tree, id), name);
                }
            }
        }
    }

    private static boolean allowedName(String value) {
        if (value == null || !value.matches("[A-Za-z0-9_.&-]{1,90}")) return false;
        String name = value.toUpperCase(Locale.US);
        if (name.startsWith("SAVE") && name.endsWith(".ITD")) return false;
        return name.endsWith(".PAK") || name.endsWith(".ITD") ||
               name.endsWith(".TAT") || name.endsWith(".CC1") ||
               name.endsWith(".BBS") || name.endsWith(".COM") ||
               name.endsWith(".BIN");
    }

    private void copyDocument(Uri uri, String displayName) throws Exception {
        if (uri == null) return;
        if (copiedFiles >= MAX_FILES) throw new IllegalStateException("Mais de 350 arquivos");
        String name = displayName;
        if (name == null) {
            // API19 document providers do not always provide a filename in URI.
            try (Cursor cursor = getContentResolver().query(uri,
                    new String[]{android.provider.OpenableColumns.DISPLAY_NAME},
                    null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) name = cursor.getString(0);
            }
        }
        if (!allowedName(name)) return;
        String safeName = name.toUpperCase(Locale.US);
        File out = new File(getFilesDir(), safeName);
        File temp = new File(getFilesDir(), safeName + ".importing");
        if (temp.exists() && !temp.delete())
            throw new IllegalStateException("Não foi possível limpar arquivo temporário");
        long length = 0;
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IllegalArgumentException("Arquivo inacessível: " + safeName);
            try (OutputStream destination = new FileOutputStream(temp)) {
                byte[] buffer = new byte[32768];
                int n;
                while ((n = in.read(buffer)) != -1) {
                    if (length + n > MAX_SINGLE_FILE || copiedBytes + length + n > MAX_TOTAL_BYTES)
                        throw new IllegalStateException("Limite de tamanho excedido: " + safeName);
                    destination.write(buffer, 0, n);
                    length += n;
                }
            }
            if (length <= 0) throw new IllegalArgumentException("Arquivo vazio: " + safeName);
            if (out.exists() && !out.delete()) throw new IllegalStateException("Não consegui substituir " + safeName);
            if (!temp.renameTo(out)) throw new IllegalStateException("Não consegui finalizar " + safeName);
            copiedFiles++;
            copiedBytes += length;
        } finally {
            if (temp.exists()) temp.delete();
        }
    }
}

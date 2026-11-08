package org.libsdl.app;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

/**
 * Android TV / phone Activity: touch controls are automatic on devices that
 * advertise a real touchscreen. Remote and Bluetooth inputs remain supported.
 * BACK never silently kills the process: display confirmation, BACK again exits.
 */
public final class FITDTVActivity extends SDLActivity {
    private static final String TAG="FITD-TV";
    private AlertDialog exitDialog;
    private boolean exitRequested=false;
    private native void nativeSetTouchEnabled(int enabled);
    private native void nativeRequestExit();

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        final PackageManager pm=getPackageManager();
        final boolean touch=pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
            || pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN_MULTITOUCH);
        try {
            nativeSetTouchEnabled(touch?1:0);
            Log.i(TAG,"FITD-TOUCH: hardware="+(touch?"present":"absent"));
        } catch(UnsatisfiedLinkError e) {
            Log.e(TAG,"FITD-TOUCH JNI not available",e);
        }
    }
    private void requestExit() {
        if(exitRequested)return;
        exitRequested=true;
        if(exitDialog!=null && exitDialog.isShowing())exitDialog.dismiss();
        try{
            nativeRequestExit();
        } catch(UnsatisfiedLinkError e){
            Log.e(TAG,"FITD native exit not available",e);
        }
        finish();
    }
    private void confirmExit() {
        if(exitRequested)return;
        if(exitDialog!=null && exitDialog.isShowing()) {
            requestExit();
            return;
        }
        exitDialog=new AlertDialog.Builder(this)
            .setTitle("Sair do Alone in the Dark?")
            .setMessage("Quer realmente sair do jogo? Pressione Voltar novamente ou escolha Sair. Para continuar, escolha Cancelar.")
            .setPositiveButton("Sair",new DialogInterface.OnClickListener(){
                @Override public void onClick(DialogInterface dialog,int which){requestExit();}
            })
            .setNegativeButton("Cancelar",new DialogInterface.OnClickListener(){
                @Override public void onClick(DialogInterface dialog,int which){exitDialog=null;}
            })
            .create();
        exitDialog.setOnKeyListener(new DialogInterface.OnKeyListener(){
            @Override public boolean onKey(DialogInterface dialog,int code,KeyEvent event){
                if(code==KeyEvent.KEYCODE_BACK){
                    if(event.getAction()==KeyEvent.ACTION_UP)requestExit();
                    return true;
                }
                return false;
            }
        });
        exitDialog.setOnDismissListener(new DialogInterface.OnDismissListener(){
            @Override public void onDismiss(DialogInterface d){exitDialog=null;}
        });
        exitDialog.show();
    }
    @Override public boolean dispatchKeyEvent(KeyEvent event) {
        if(event.getKeyCode()==KeyEvent.KEYCODE_BACK){
            if(event.getAction()==KeyEvent.ACTION_UP)confirmExit();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
    @Override public void onBackPressed(){confirmExit();}
}

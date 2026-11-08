package org.libsdl.app;

import android.app.AlertDialog;
import android.content.pm.ActivityInfo;
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
    private volatile boolean exitRequested=false;
    private static volatile FITDTVActivity currentInstance;
    private native void nativeSetTouchEnabled(int enabled);
    private native void nativeRequestExit();

    @Override protected void onCreate(Bundle state) {
        super.onCreate(state);
        currentInstance=this;
        // FULL_USER follows the Android auto-rotate switch. Unlike FULL_SENSOR,
        // it also respects the user's orientation lock.
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);
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
    @Override protected void onResume() {
        super.onResume();
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);
    }
    // SDLActivity can outlive its Java window while the upstream FitdMain
    // worker is still running. This leaves SDL/EGL and static Java state
    // attached to the previous run and makes the very next launch close.
    // On a confirmed user exit, first request graceful shutdown. The
    // process must then finish, with a bounded native-thread watchdog.
    private void requestExit() {
        if(exitRequested)return;
        exitRequested=true;
        if(exitDialog!=null && exitDialog.isShowing())exitDialog.dismiss();
        try{
            nativeRequestExit();
        } catch(UnsatisfiedLinkError e){
            Log.e(TAG,"FITD native exit not available",e);
        }
        final FITDTVActivity leaving=this;
        Thread shutdownGuard=new Thread(new Runnable(){
            @Override public void run(){
                try{Thread.sleep(1800);}catch(InterruptedException ignored){}
                if(currentInstance==leaving && leaving.exitRequested){
                    Log.w(TAG,"FITD-EXIT: native shutdown timed out; terminating old process to allow clean relaunch");
                    android.os.Process.killProcess(android.os.Process.myPid());
                }
            }
        },"FITD-exit-watchdog");
        shutdownGuard.setDaemon(true);
        shutdownGuard.start();
        finish();
    }

    @Override protected void onDestroy(){
        try{
            super.onDestroy();
        }finally{
            if(exitRequested && currentInstance==this){
                Log.i(TAG,"FITD-EXIT: SDL2 shutdown completed; ending old process");
                android.os.Process.killProcess(android.os.Process.myPid());
            }else if(currentInstance==this){
                currentInstance=null;
            }
        }
    }

    @Override public void setOrientationBis(int w,int h,boolean resizable,String hint){
        // SDL2's default fixed 640x480 fullscreen surface is treated as
        // landscape and overrides the manifest. Always honor the Android
        // user's auto-rotate setting on the actual gameplay Activity.
        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_FULL_USER);
        Log.i(TAG,"FITD-ROTATION: allow landscape/portrait w="+w+" h="+h+" resizable="+resizable);
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

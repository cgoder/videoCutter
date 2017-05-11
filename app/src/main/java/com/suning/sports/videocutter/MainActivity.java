package com.suning.sports.videocutter;

import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.MediaStore;
import android.widget.MediaController;
import android.widget.TextView;
import android.support.design.widget.FloatingActionButton;
import android.support.design.widget.Snackbar;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.Toolbar;
import android.view.View;
import android.view.Menu;
import android.view.MenuItem;
import android.widget.VideoView;

@SuppressWarnings("ResourceAsColor")
public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("ffmpeg");
        System.loadLibrary("native-lib");
    }
//    int clip_stats =0;
    String inFile,outFile;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        VideoView videoView = (VideoView) findViewById(R.id.videoView);
        MediaController mc = new MediaController(this);
        videoView.setMediaController(mc);

        FloatingActionButton fab = (FloatingActionButton) findViewById(R.id.fab);
        fab.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                Intent intent = new Intent(Intent.ACTION_PICK, MediaStore.Video.Media.EXTERNAL_CONTENT_URI);
                startActivityForResult(intent, 100);
            }
        });


        FloatingActionButton recbutton = (FloatingActionButton) findViewById(R.id.clipbutton);
        recbutton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if ((null != inFile)) {
//                    outFile = inFile+".mp4";
                    outFile = "/storage/emulated/0/clip.mp4";
                    ffmpeg_clip_do(10d, 20d, inFile, outFile);
                }
            }
        });

        FloatingActionButton remuxbutton = (FloatingActionButton) findViewById(R.id.remuxbutton);
        remuxbutton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if ((null != inFile)) {
//                    outFile = inFile+".flv";
                    outFile = "/storage/emulated/0/remux.flv";
                    ffmpeg_remux_do(inFile, outFile);
                }
            }
        });

        ffmpegInit();

    }

    /**
     * URI->path
     */
    public static String getRealFilePath(final Context context, final Uri uri ) {
        if ( null == uri ) return null;
        final String scheme = uri.getScheme();
        String data = null;
        if ( scheme == null )
            data = uri.getPath();
        else if ( ContentResolver.SCHEME_FILE.equals( scheme ) ) {
            data = uri.getPath();
        } else if ( ContentResolver.SCHEME_CONTENT.equals( scheme ) ) {
            Cursor cursor = context.getContentResolver().query( uri, new String[] { MediaStore.Images.ImageColumns.DATA }, null, null, null );
            if ( null != cursor ) {
                if ( cursor.moveToFirst() ) {
                    int index = cursor.getColumnIndex( MediaStore.Images.ImageColumns.DATA );
                    if ( index > -1 ) {
                        data = cursor.getString( index );
                    }
                }
                cursor.close();
            }
        }
        return data;
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 100 && resultCode == RESULT_OK) {
            Uri uri = data.getData();
            if (uri != null) {
                inFile = getRealFilePath(this,uri);
                VideoView videoView = (VideoView) findViewById(R.id.videoView);
                videoView.setVideoURI(uri);
                videoView.start();
            }
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */

    public native String ffmpegInit();
    public native int ffmpeg_clip_do(double startTime, double endTime,  String inFile, String outFile);
    public native int ffmpeg_remux_do(String inFile, String outFile);
}

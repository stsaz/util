/** phiola/Android
2022, Simon Zolin */

package com.github.stsaz.phiola;

import androidx.annotation.NonNull;

import android.app.Activity;
import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.util.Log;

import androidx.core.app.ActivityCompat;

class Core extends Util {
	private static Core instance;
	private int refcount;

	private static final String TAG = "phiola.Core";
	static String PUB_DATA_DIR = "phiola";

	Phiola phiola;
	UtilNative util;
	private Conf conf;
	Handler tq;

	String storage_path;
	String[] storage_paths;
	String work_dir;
	Context context;

	static Core getInstance() {
		instance.dbglog(TAG, "getInstance");
		instance.refcount++;
		return instance;
	}

	static Core init_once(Context ctx) {
		if (instance == null) {
			if (BuildConfig.DEBUG)
				PUB_DATA_DIR = "phiola-dbg";
			Core c = new Core();
			c.refcount = 1;
			c.init(ctx);
			instance = c;
			return c;
		}
		return getInstance();
	}

	private void init(@NonNull Context ctx) {
		dbglog(TAG, "init");
		context = ctx;
		work_dir = ctx.getFilesDir().getPath();
		storage_path = Environment.getExternalStorageDirectory().getPath();
		storage_paths = system_storage_dirs(ctx);

		tq = new Handler(Looper.getMainLooper());
		phiola = new Phiola(ctx.getApplicationInfo().nativeLibraryDir, ctx.getAssets());
		conf = new Conf(phiola);
		util = new UtilNative(phiola);
		util.storagePaths(storage_paths);
	}

	void unref() {
		dbglog(TAG, "unref(): %d", refcount);
		refcount--;
	}

	void close() {
		dbglog(TAG, "close(): %d", refcount);
		if (--refcount != 0)
			return;
		instance = null;
		phiola.destroy();
	}

	boolean sys_permisson_request(Activity activity, String[] perms, int code, int ext_stg_mgr_req_code) {
		boolean r = true;

		for (String p : perms) {
			if (ActivityCompat.checkSelfPermission(activity, p) != PackageManager.PERMISSION_GRANTED) {
				ActivityCompat.requestPermissions(activity, perms, code);
				r = false;
				break;
			}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
			if (ext_stg_mgr_req_code != 0 && !Environment.isExternalStorageManager()) {
				Intent it = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION, Uri.parse("package:" + BuildConfig.APPLICATION_ID));
				ActivityCompat.startActivityForResult(activity, it, ext_stg_mgr_req_code, null);
				r = false;
			}
		}

		return r;
	}

	void clipboard_text_set(Context ctx, String s) {
		ClipboardManager cm = (ClipboardManager)ctx.getSystemService(Context.CLIPBOARD_SERVICE);
		ClipData cd = ClipData.newPlainText("", s);
		cm.setPrimaryClip(cd);
	}

	void errlog(String mod, String fmt, Object... args) {
		Log.e(mod, String.format("%s: %s", mod, String.format(fmt, args)));
		if (gui != null)
			gui.on_error(fmt, args);
	}

	void dbglog(String mod, String fmt, Object... args) {
		if (BuildConfig.DEBUG)
			Log.d(mod, String.format("%s: %s", mod, String.format(fmt, args)));
	}
}

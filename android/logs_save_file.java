private void logs_save_file() {
	String fn = String.format("%s/logs.txt", core.setts.pub_data_dir);
	String[] args = new String[]{
		"logcat", "-f", fn, "-d"
	};
	try {
		Runtime.getRuntime().exec(args);
		core.gui().msg_show(this, getString(R.string.about_log_saved), fn);
	} catch (Exception e) {
		core.errlog(TAG, "logs_save_file: %s", e);
	}
}

// 文件分布：frameworks/base/services/core/java/com/android/server/am/ActivityManagerService.java

    ProcessRecord getRecordForAppLocked(IApplicationThread thread) {
        if (thread == null) {
            return null;
        }

        ProcessRecord record = mProcessList.getLRURecordForAppLocked(thread);
        if (record != null) return record;

        // Validation: if it isn't in the LRU list, it shouldn't exist, but let's
        // double-check that.
        final IBinder threadBinder = thread.asBinder();
        final ArrayMap<String, SparseArray<ProcessRecord>> pmap =
                mProcessList.mProcessNames.getMap();
        for (int i = pmap.size()-1; i >= 0; i--) {
            final SparseArray<ProcessRecord> procs = pmap.valueAt(i);
            for (int j = procs.size()-1; j >= 0; j--) {
                final ProcessRecord proc = procs.valueAt(j);
                if (proc.thread != null && proc.thread.asBinder() == threadBinder) {
                    Slog.wtf(TAG, "getRecordForApp: exists in name list but not in LRU list: "
                            + proc);
                    return proc;
                }
            }
        }

        return null;
    }

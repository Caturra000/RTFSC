// 文件分布：frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java

/** Root {@link WindowContainer} for the device. */
class RootWindowContainer extends WindowContainer<DisplayContent>
        implements DisplayManager.DisplayListener {

    ActivityRecord isInAnyStack(IBinder token) {
        final ActivityRecord r = ActivityRecord.forTokenLocked(token);
        return (r != null && r.isDescendantOf(this)) ? r : null;
    }

}
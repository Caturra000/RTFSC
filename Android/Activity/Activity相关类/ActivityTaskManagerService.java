// 文件分布：frameworks/base/services/core/java/com/android/server/wm/ActivityTaskManagerService.java


/**
 * System service for managing activities and their containers (task, stacks, displays,... ).
 *
 * {@hide}
 */
public class ActivityTaskManagerService extends IActivityTaskManager.Stub {


    /** @see WindowSurfacePlacer#deferLayout */
    void deferWindowLayout() {
        if (!mWindowManager.mWindowPlacerLocked.isLayoutDeferred()) {
            // Reset the reasons at the first entrance because we only care about the changes in the
            // deferred scope.
            mLayoutReasons = 0;
        }

        mWindowManager.mWindowPlacerLocked.deferLayout();
    }

}
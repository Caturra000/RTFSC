// 文件分布：frameworks/base/services/core/java/com/android/server/wm/WindowSurfacePlacer.java

/**
 * Positions windows and their surfaces.
 *
 * It sets positions of windows by calculating their frames and then applies this by positioning
 * surfaces according to these frames. Z layer is still assigned withing WindowManagerService.
 */
class WindowSurfacePlacer {
    
    /**
     * Starts deferring layout passes. Useful when doing multiple changes but to optimize
     * performance, only one layout pass should be done. This can be called multiple times, and
     * layouting will be resumed once the last caller has called {@link #continueLayout}.
     */
    void deferLayout() {
        mDeferDepth++;
    }


}
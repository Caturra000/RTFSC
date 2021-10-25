// 文件：frameworks/base/services/core/java/com/android/server/wm/RootWindowContainer.java

    boolean resumeFocusedStacksTopActivities() {
        return resumeFocusedStacksTopActivities(null, null, null);
    }

    boolean resumeFocusedStacksTopActivities(
            ActivityStack targetStack, ActivityRecord target, ActivityOptions targetOptions) {

        if (!mStackSupervisor.readyToResume()) {
            return false;
        }

        boolean result = false;
        if (targetStack != null && (targetStack.isTopStackInDisplayArea()
                || getTopDisplayFocusedStack() == targetStack)) {
            result = targetStack.resumeTopActivityUncheckedLocked(target, targetOptions);
        }

        for (int displayNdx = getChildCount() - 1; displayNdx >= 0; --displayNdx) {
            final DisplayContent display = getChildAt(displayNdx);
            boolean resumedOnDisplay = false;
            for (int tdaNdx = display.getTaskDisplayAreaCount() - 1; tdaNdx >= 0; --tdaNdx) {
                final TaskDisplayArea taskDisplayArea = display.getTaskDisplayAreaAt(tdaNdx);
                for (int sNdx = taskDisplayArea.getStackCount() - 1; sNdx >= 0; --sNdx) {
                    final ActivityStack stack = taskDisplayArea.getStackAt(sNdx);
                    final ActivityRecord topRunningActivity = stack.topRunningActivity();
                    if (!stack.isFocusableAndVisible() || topRunningActivity == null) {
                        continue;
                    }
                    if (stack == targetStack) {
                        // Simply update the result for targetStack because the targetStack had
                        // already resumed in above. We don't want to resume it again, especially in
                        // some cases, it would cause a second launch failure if app process was
                        // dead.
                        resumedOnDisplay |= result;
                        continue;
                    }
                    if (taskDisplayArea.isTopStack(stack) && topRunningActivity.isState(RESUMED)) {
                        // Kick off any lingering app transitions form the MoveTaskToFront
                        // operation, but only consider the top task and stack on that display.
                        stack.executeAppTransition(targetOptions);
                    } else {
                        resumedOnDisplay |= topRunningActivity.makeActiveIfNeeded(target);
                    }
                }
            }
            if (!resumedOnDisplay) {
                // In cases when there are no valid activities (e.g. device just booted or launcher
                // crashed) it's possible that nothing was resumed on a display. Requesting resume
                // of top activity in focused stack explicitly will make sure that at least home
                // activity is started and resumed, and no recursion occurs.
                final ActivityStack focusedStack = display.getFocusedStack();
                if (focusedStack != null) {
                    result |= focusedStack.resumeTopActivityUncheckedLocked(target, targetOptions);
                } else if (targetStack == null) {
                    result |= resumeHomeActivity(null /* prev */, "no-focusable-task",
                            display.getDefaultTaskDisplayArea());
                }
            }
        }

        return result;
    }
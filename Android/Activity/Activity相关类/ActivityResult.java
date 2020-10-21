
    // 文件分布： frameworks/base/core/java/android/app/Instrumentation.java


    // TL;DR Activity的result只有int(code)和Intent(Data)


    /**
     * Description of a Activity execution result to return to the original
     * activity.
     */
    public static final class ActivityResult {
        /**
         * Create a new activity result.  See {@link Activity#setResult} for 
         * more information. 
         *  
         * @param resultCode The result code to propagate back to the
         * originating activity, often RESULT_CANCELED or RESULT_OK
         * @param resultData The data to propagate back to the originating
         * activity.
         */
        public ActivityResult(int resultCode, Intent resultData) {
            mResultCode = resultCode;
            mResultData = resultData;
        }

        /**
         * Retrieve the result code contained in this result.
         */
        public int getResultCode() {
            return mResultCode;
        }

        /**
         * Retrieve the data contained in this result.
         */
        public Intent getResultData() {
            return mResultData;
        }

        private final int mResultCode;
        private final Intent mResultData;
    }

// 文件：frameworks/base/core/java/android/app/AppComponentFactory.java

    /**
     * Allows application to override the creation of activities. This can be used to
     * perform things such as dependency injection or class loader changes to these
     * classes.
     * <p>
     * This method is only intended to provide a hook for instantiation. It does not provide
     * earlier access to the Activity object. The returned object will not be initialized
     * as a Context yet and should not be used to interact with other android APIs.
     *
     * @param cl        The default classloader to use for instantiation.
     * @param className The class to be instantiated.
     * @param intent    Intent creating the class.
     */
    public @NonNull Activity instantiateActivity(@NonNull ClassLoader cl, @NonNull String className,
            @Nullable Intent intent)
            throws InstantiationException, IllegalAccessException, ClassNotFoundException {
        return (Activity) cl.loadClass(className).newInstance();
    }
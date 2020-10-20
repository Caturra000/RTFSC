
// 这里记录一些Broadcast实现过程中用到的类

// 允许Binder
public interface IIntentReceiver {
    public void asBinder();
    public void performReceive(Intent intent, String data, Bundle extras
                                boolean ordered, boolean sticky, int sendingUser);
}



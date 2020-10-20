
// 允许Binder
public interface IIntentReceiver {
    public void asBinder();
    public void performReceive(Intent intent, String data, Bundle extras
                                boolean ordered, boolean sticky, int sendingUser);
}



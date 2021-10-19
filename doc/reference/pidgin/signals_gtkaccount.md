Title: Account Signals
Slug: account-signals

## Account Signals

### account-modified

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when the settings for an account have been changed and saved.

**Parameters:**

**account**
: The account that has been modified.

**user_data**
: User data set when the signal handler was connected.


Title: Account Signals
Slug: account-signals

## Account Signals

### Signal List

* [account-created](#account-created)
* [account-destroying](#account-destroying)
* [account-added](#account-added)
* [account-connecting](#account-connecting)
* [account-removed](#account-removed)
* [account-disabled](#account-disabled)
* [account-enabled](#account-enabled)
* [account-setting-info](#account-setting-info)
* [account-set-info](#account-set-info)
* [account-status-changed](#account-status-changed)
* [account-actions-changed](#account-actions-changed)
* [account-alias-changed](#account-alias-changed)
* [account-authorization-requested](#account-authorization-requested)
* [account-authorization-denied](#account-authorization-denied)
* [account-authorization-granted](#account-authorization-granted)
* [account-error-changed](#account-error-changed)
* [account-signed-on](#account-signed-on)
* [account-signed-off](#account-signed-off)
* [account-connection-error](#account-connection-error)

### Signal Details

#### account-created

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is created by calling purple_account_new.

**Parameters:**

**account**
: The account.

**user_data**
: User data set when the signal handler was connected.

----

#### account-destroying

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is about to be destroyed.

**Parameters:**

**account**
: The account.

**user_data**
: User data set when the signal handler was connected.

----

#### account-added

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is added.

**Parameters:**


**account**
: The account that was added. See `purple_accounts_add()`.

**user_data**
: User data set when the signal handler was connected.

----

#### account-connecting

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

This is emitted when an account is in the process of connecting.

**Parameters:**

**account**
: The account in the process of connecting.

**user_data**
: User data set when the signal handler was connected.

----

#### account-removed

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is removed.

**Parameters:**

**account**
: The account that was removed. See `purple_accounts_remove()`.

**user_data**
: User data set when the signal handler was connected.

----

#### account-disabled

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is disabled.

**Parameters:**

**account**
: The account that was disabled.

**user_data**
: User data set when the signal handler was connected.

----

#### account-enabled

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account is enabled.

**Parameters**:

**account**
: The account that was enabled.

**user_data**
: User data set when the signal handler was connected.

----

#### account-setting-info

```c
void user_function(PurpleAccount *account, const gchar *new_info, gpointer user_data);
```

Emitted when a user is about to send his new user info, or profile, to the server.

**Parameters:**

**account**
: The account that the info will be set on.

**new_info**
: The new information to set.

**user_data**
: User data set when the signal handler was connected.

----

#### account-set-info

```c
void user_function(PurpleAccount *account, const gchar *new_info, gpointer user_data);
```

Emitted when a user sent his new user info, or profile, to the server.

**Parameters:**

**account**
: The account that the info was set on.

**new_info**
: The new information set.

**user_data**
: User data set when the signal handler was connected.

----

#### account-status-changed

```c
void user_function(PurpleAccount *account,
                   PurpleStatus *old,
                   PurpleStatus *new,
                   gpointer user_data);
```

Emitted when the status of an account changes (after the change).

**Parameters:**

**account**
: The account that changed status.

**old**
: The status before change.

**new**
: The status after change.

**user_data**
: User data set when the signal handler was connected.

----

#### account-actions-changed

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when the account actions are changed after initial connection.

**Parameters:**

**account**
: The account whose actions changed.

**user_data**
: User data set when the signal handler was connected.

----

#### account-alias-changed

```c
void user_function(PurpleAccount *account, const gchar *old, gpointer user_data);
```

Emitted when the alias of an account changes (after the change).

**Parameters:**

**account**
: The account for which the alias was changed.

**old**
: The alias before change.

**user_data**
: User data set when the signal handler was connected.

----

#### account-authorization-requested

```c
int user_function(PurpleAccount *account,
                  const gchar *user,
                  const gchar *message,
                  gchar **response,
                  gpointer user_data);
```

Emitted when a user requests authorization.

**Parameters:**

**account**
: The account.

**user**
: The name of the user requesting authorization.

**message**
: The authorization request message.

**response**
: The message to send in the response.

**user_data**
: User data set when the signal handler was connected.

**Returns:**

`PURPLE_ACCOUNT_RESPONSE_IGNORE`
: To silently ignore the request

`PURPLE_ACCOUNT_RESPONSE_DENY`
: To block the request (the sender might get informed)

`PURPLE_ACCOUNT_RESPONSE_ACCEPT`
: If the request should be granted.

`PURPLE_ACCOUNT_RESPONSE_PASS`
: The user will be prompted with the request.

----

#### account-authorization-denied

```c
void user_function(PurpleAccount *account,
                   const gchar *user,
                   const gchar *message,
                   gpointer user_data);
```

Emitted when the authorization request for a buddy is denied.

**Parameters:**

**account**
: The account.

**user**
: The name of the user requesting authorization.

**message**
: The message to tell the buddy who was denied.

**user_data**
: User data set when the signal handler was connected.

----

#### account-authorization-granted

```c
void user_function(PurpleAccount *account,
                   const gchar *user,
                   const gchar *message,
                   gpointer user_data);
```

Emitted when the authorization request for a buddy is granted.

**Paramaters:**

**account**
: The account.

**user**
: The name of the user requesting authorization.

**message**
: The message to tell the buddy who was granted authorization.

**user_data**
: User data set when the signal handler was connected.

----

#### account-error-changed

```c
void user_function(PurpleAccount *account,
                   const PurpleConnectionErrorInfo *old_error,
                   const PurpleConnectionErrorInfo *current_error,
                   gpointer user_data);
```

Emitted when `account`'s error changes.  You should not call purple_account_clear_current_error() while this signal is being emitted.

**Parameters:**

**account**
: The account whose error has changed.

**old_error**
: The account's previous error, or `NULL` if it had no error.  After this signal is emitted, `old_error` is not guaranteed to be a valid pointer.

**new_error**
: The account's new error, or `NULL` if it has no error. If not `NULL`, `new_error` will remain a valid until pointer just after the next time this signal is emitted for this `account`. See `purple_account_get_current_error()`.

**user_data**
: User data set when the signal handler was connected.

----

#### account-signed-on

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account has signed on.

**Parameters:**

**account**
: The account that has signed on.

**user_data**
: User data set when the signal handler was connected.

----

#### account-signed-off

```c
void user_function(PurpleAccount *account, gpointer user_data);
```

Emitted when an account has signed off.

**Parameters:**

**account**
: The account that has signed off.

**user_data**
: User data set when the signal handler was connected.

----

#### account-connection-error

```c
void user_function(PurpleAccount *account,
                   PurpleConnectionError err,
                   const gchar *desc,
                   gpointer user_data)
```

Emitted when a connection error occurs, before `"signed"`-off.

**Parameters:**

**account**
: The account on which the error has occurred.

**err**
: The error that occurred.

**desc**
: A description of the error, giving more information.

**user_data**
: User data set when the signal handler was connected.

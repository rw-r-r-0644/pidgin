Title: Notify Signals
Slug: notify-signals

## Notify Signals

### Signal List

* [displaying-userinfo](#displaying-userinfo)
* [displaying-email-notification](#displaying-email-notification)
* [displaying-emails-notification](#displaying-emails-notification)

### Signal Details

#### displaying-userinfo

```c
void user_function(PurpleAccount *account,
                   const gchar *who,
                   PurpleNotifyUserInfo *user_info,
                   gpointer user_data);
```

Emitted before userinfo is handed to the UI to display. `user_info` can be manipulated via the PurpleNotifyUserInfo API in notify.c.

> If adding a PurpleNotifyUserInfoEntry, be sure not to free it --
> PurpleNotifyUserInfo assumes responsibility for its objects.

**Parameters:**

**account**
: The account on which the info was obtained.

**who**
: The name of the buddy whose info is to be displayed.

**user_info**
: The information to be displayed, as PurpleNotifyUserInfoEntry objects.

**user_data**
: user data set when the signal handler was connected.

----

#### displaying-email-notification

```c
void user_function(const gchar *subject,
                   const gchar *from,
                   const gchar *to,
                   const gchar *url,
                   gpointer user_data);
```

Emitted before notification of a single email is handed to the UI to display.

**Parameters:**

**subject**
: Subject of email being notified of.

**from**
: Who the email is from.

**to**
: Who the email is to.

**url**
: A url to view the email.

**user_data**
: user data set when the signal handler was connected.

----

#### displaying-emails-notification

```c
void user_function(const gchar **subjects,
                   const gchar **froms,
                   const gchar **tos,
                   const gchar **urls,
                   guint count,
                   gpointer user_data)
```

Emitted before notification of multiple emails is handed to the UI to display.

**Parameters:**

**subjects**
: Subjects of emails being notified of.

**froms**
: Who the emails are from.

**tos**
: Who the emails are to.

**urls**
: The urls to view the emails.

**count**
: Number of emails being notified of.

**user_data**
: user data set when the signal handler was connected.

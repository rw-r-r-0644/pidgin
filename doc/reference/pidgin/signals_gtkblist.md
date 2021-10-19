Title: Buddy List Signals
Slug: blist-signals

## Buddy List Signals

### gtkblist-hiding

```c
void user_function(PurpleBuddyList *blist, gpointer user_data);
```

Emitted when the buddy list is about to be hidden.

**Parameters:**

**blist**
: The buddy list.

**user_data**
: User data set when the signal handler was connected.

### gtkblist-unhiding

```c
void user_function(PurpleBuddyList *blist, gpointer user_data);
```
Emitted when the buddy list is about to be unhidden.

**Parameters:**

**blist**
: The buddy list.

**user_data**
: User data set when the signal handler was connected.

### gtkblist-created

```c
void user_function(PurpleBuddyList *blist, gpointer user_data);
```

Emitted when the buddy list is created.

**Parameters:**

**blist**
: The buddy list.

**user_data**
: User data set when the signal handler was connected.

### drawing-tooltip

```c
void user_function(PurpleBlistNode *node, GString *text, gboolean full, gpointer user_data);
```

Emitted just before a tooltip is displayed. `text` is a standard GString, so
the plugin can modify the text that will be displayed.

**Parameters:**

**node**
: The blist node for the tooltip.

**text**
: A pointer to the text that will be displayed.

**full**
: Whether we're doing a full tooltip for the priority buddy or a compact
tooltip for a non-priority buddy.

**user_data**
: User data set when the signal handler was connected.

### drawing-buddy

```c
char *user_function(PurpleBuddy *buddy, gpointer user_data);
```

Emitted to allow plugins to handle markup within a buddy's name or to override
the default of no formatting for names shown in the buddy list.

**Parameters:**

**buddy**
: A pointer to the PupleBuddy that will be displayed.

**user_data**
: User data set when the signal handler was connected.

**Returns:**

The text to display (must be allocated), or `NULL` if no changes to the default
behavior are desired.


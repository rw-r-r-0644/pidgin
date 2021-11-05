Title: Using Pidgin Mercurial
Slug: using-pidgin-mercurial

## Using Pidgin Mercurial

### Introduction

These instructions will help you clone a copy of any of the Pidgin
related [Mercurial](https://mercurial-scm.org)
repositories and keep them up to date.

These instructions are just for cloning/updating the Pidgin repositories.
If you're looking for documentation on contributing code, please see the
[Code Contributions](code_contributions.html)
page after you have successfully cloned the repository from this page.

### Cloning

In Distributed Version Control, ***cloning*** is the act
of acquiring a source repository. All of the Pidgin repositories are
hosted in Mercurial at
[keep.imfreedom.org](https://keep.imfreedom.org/). To
clone them you will be using the command
`hg clone <URL>`. The specific URL can be looked up in
the table below depending what you are trying to clone.

> If you are trying to build Pidgin 3, you can just clone that repository and
> the build system will automatically clone the other repositories for you.

#### Repositories

[https://keep.imfreedom.org/gplugin/gplugin/](https://keep.imfreedom.org/gplugin/gplugin/)
: The plugin library used in Pidgin 3.

[https://keep.imfreedom.org/libgnt/libgnt/](https://keep.imfreedom.org/libgnt/libgnt/)
: The toolkit library used in Finch.

[https://keep.imfreedom.org/pidgin/pidgin/](https://keep.imfreedom.org/pidgin/pidgin/)
: The main Pidgin repository that contains LibPurple, Pidgin, and Finch.

[https://keep.imfreedom.org/talkatu/talkatu/](https://keep.imfreedom.org/talkatu/talkatu/)
: The conversation widgets used in Pidgin 3.

You can see an example clone of Talkatu below but all of the repositories will
output a similar result.

```sh
$ hg clone https://keep.imfreedom.org/talkatu/talkatu
destination directory: talkatu
requesting all changes
adding changesets
adding manifests
adding file changes
added 348 changesets with 1074 changes to 268 files
new changesets 0feed1461a4a:f0fda4aace2d
updating to branch default
109 files updated, 0 files merged, 0 files removed, 0 files unresolved
```

### Keeping Your Clone Up To Date

If you are just tracking Pidgin development and are not contributing, chances
are you are still on the ***default*** branch. But let's make sure, and run
`hg update default`.  This will change to the ***default*** branch if you're
not currently on it or do nothing.

Now that you are on the ***default*** branch, you can
simply run `hg pull --update` to pull in all new changes and
update your local copy. Please note, if you accidentally run
`hg pull`, that is without the update, a subsequent
`hg pull --update` will not update to the latest revisions as
this invocation of `hg pull` did not find any new revisions. To
properly update in this scenario, you'll need to run
`hg update`.

Below is an example of updating Talkatu when it's already up to date.

```sh
$ hg pull --update
pulling from https://keep.imfreedom.org/talkatu/talkatu
searching for changes
no changes found
```

At this point you can review the code, build it, patch it, etc.

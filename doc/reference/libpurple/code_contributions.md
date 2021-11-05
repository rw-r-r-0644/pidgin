Title: Code Contributions
Slug: code-contributions

## Code Contributions

### Introduction

All of the Pidgin related projects use [Review
Board](https://reviewboard.org/) for handling contributions at
[reviews.imfreedom.org](https://reviews.imfreedom.org).

### First Time Setup

There are a few things you'll need to set up to be able to submit a code
review to these projects. This includes installing
[RBTools](https://www.reviewboard.org/downloads/rbtools/)
as well as some additional
[Mercurial](https://www.mercurial-scm.org/)
configuration.

#### Install RBTools

The recommended way to install RBTools is via pip and can be done with the
following command.

```sh
pip3 install -U "RBTools>=1.0.3"
```
Once RBTools is installed you need to make sure that `rbt` is available on your
`$PATH`. To do this, you may need to add `$HOME/.local/bin` to your `$PATH`.
The exact procedure to do this is dependent on your setup and outside of the
scope of this document.

#### Mercurial Configuration

This configuration for Mercurial is to make your life as a contributor easier.
There a few different ways to configure Mercurial, but these instructions will
update your user specific configuration in `$HOME/.hgrc`.

The first thing we need to do is to install the evolve extension. This
extension makes rewriting history safe and we use it extensively in our
repositories. You can install it with a simple `pip3 install -U hg-evolve`. We
will enable it below with some other bundled extensions, but you can find more
information about it
[here](https://www.mercurial-scm.org/wiki/EvolveExtension).

When working with Mercurial repositories it is very important to make sure that
your username is set properly as it is added to every commit you make. To set
your username you must add it to the `[ui]` section in your `$HOME/.hgrc` like
the following example.

```ini
[ui]
username = Full Name <email@example.com>
```

Next we need to make sure that the ***evolve*** and ***rebase*** extensions are
loaded. To do so add the lines in the following example. You do not need to put
anything after the `=` as this will tell Mercurial to look for them in the
default places for extensions.

```ini
[extensions]
evolve =
rebase =
```

Next we're going to create a ***revsetalias***. This will be used to make it
easier to look at your history and submit your review request.

```ini
[revsetalias]
wip = only(.,default)
```

This alias will show us just the commits that are on our working branch and not
on the default branch. The default branch is where all accepted code
contributions go. Optionally, you can add the `wip` command alias below which
will show you the revision history of what you are working on.

```ini
[alias]
wip = log --graph --rev wip
```

There are quite a few other useful configuration changes you can make, and a
few examples can be found below.

```ini
[ui]
# update a large number of settings for a better user experience, highly
# recommended!!
tweakdefaults = true

[alias]
# make hg log show the graph as well as commit phase
lg = log --graph --template phases
```

Below is all of the above configuration settings to make it easier to
copy/paste.

```ini
[ui]
username = Full Name <email@example.com>
# update a large number of settings for a better user experience, highly
# recommended!!
tweakdefaults = true

[extensions]
evolve =
rebase =

[alias]
# make hg log show the graph as well as commit phase
lg = log --graph --template phases

# show everything between the upstream and your wip
wip = log --graph --rev wip

[revsetalias]
wip = only(.,default)
```

#### Log in to Review Board

To be able to submit a review request you need to have an account on our
JetBrains Hub instance at [hub.imfreedom.org](https://hub.imfreedom.org). You
can create an account here in a number of ways and even turn on two factor
authentication. But please note that if you turn on two factor authentication
you will need to create an [application
password](https://www.jetbrains.com/help/hub/application-passwords.html) to be
able to login to Review Board.

Once you have that account you can use it to login our Review Board instance at
[reviews.imfreedom.org](https://reviews.imfreedom.org).  Please note, you will
have to login via the web interface before being able to use RBTools.

Once you have an account and have logged into our Review Board site, you can
begin using RBTools. In your shell, navigate to a Mercurial clone of one of the
Pidgin or purple-related projects, then run the `rbt login` command. You should
only need to do this once, unless you change your password or have run the `rbt
logout` command.

### Creating a New Review Request

Before starting a new review request, you should make sure that your
local copy of the repository is up to date. To do so, make sure you are
on the ***default*** branch via
`hg update default`. Once you are on the
***default*** branch, you can update your copy with
`hg pull --update`. Now that you're starting with the most
recent code, you can proceed with your contributions.

While it's not mandatory, it is highly recommended that you work on your
contributions via a branch. If you don't go this path, you will have
issues after your review request is merged. This branch name can be
whatever you like as it will not end up in the main repositories, and
you can delete it from your local repository after it is merged. See
[cleanup](#cleanup) for more information.

You can create the branch with the following command:

```sh
hg branch my-new-branch-name
```

Now that you have a branch started, you can go ahead and work like you normally
would, committing your code at logical times, etc. Once you have some work
committed and you are ready to create a new review request, you can type `rbt
post wip` and you should be good to go. This will create a new review request
using all of the committed work in your repository and will output something
like below.

```sh
Review request #403 posted.

https://reviews.imfreedom.org/r/403/
https://reviews.imfreedom.org/r/403/diff/
```

At this point, your review request has been posted, but it is not yet
published. This means no one can review it yet. To do that, you need to go to
the URL that was output from your `rbt post` command and verify that everything
looks correct. If this review request fixes any bugs, please make sure to enter
their numbers in the bugs field on the right. Also, be sure to review the
actual diff yourself to make sure it includes what you intended it to and
nothing extra.

Once you are happy with the review request, you can hit the publish button
which will make the review request public and alert the reviewers of its
creation. Optionally you can pass `--open` to `rbt post` in the future to
automatically open the draft review in your web browser.

`rbt post` has a ton of options, so be sure to check them out with `rbt post
--help`. There are even options to automatically fill out the bugs fixed fields
among other things.

### Updating an Existing Review Request

Typically with a code review, you're going to need to make some updates.
However there's also a good chance that your original branching point has
changed as other contributions are accepted. To deal with this you'll need to
rebase your branch on top of the new changes.

Rebasing, as the name suggests is the act of replaying your previous
commits on top of a new base revision. Mercurial makes this pretty easy.
First, make sure you are on your branch with
`hg up my-branch-name`.  Now you can preview the rebase with
`hg rebase -d default --keepbranches --dry-run`. We prefer
doing a dry-run just to make sure there aren't any major surprises. You
may run into some conflicts, but those will have to be fixed regardless.

If everything looks good, you can run the actual rebase with `hg rebase -d
default --keepbranches`. Again if you run into any conflicts, you will have to
resolve them and they will cause the dry-run to fail. Once you have fixed the
merge conflicts, you'll then need to mark the files as resolved with `hg
resolve --mark filename`. When you have resolved all of the conflicted files
you can continue the rebase with `hg rebase --continue`. You may run into
multiple conflicts, so just repeat until you're done.

After rebasing you can start addressing the comments in your review and commit
them.  Once they are committed, you can update your existing review request
with `rbt post --update`. If for some reason `rbt` can not figure out the
proper review request to update, you can pass the number in via `rbt post
--review-request-id #`. Note that when using `--review-request-id` you no
longer need to specify `--update`.

Just like an initial `rbt post`, the updated version will be in a draft state
until you publish it. So again, you'll need to visit the URL that was output,
verify everything, and click the publish button.

### Landing a Review Request

This will typically only be done by the Pidgin developers with push access. If
you want to test a patch from a review request, please see the [patch](#path)
section below.

It is ***HIGHLY*** recommended that you use a separate clone of the repository
in question when you want to land review requests.  This makes it much easier
to avoid accidentally pushing development work to the canonical repository
which makes everyone's life easier. Also, the mainline repositories now auto
publish, so if you do not selectively push commits, all of your draft commits
will be published. You can name this additional clone whatever you like, but
using something like `pidgin-clean` is a fairly common practice. This makes it
easy for you to know that this clone is only meant for landing review requests,
and other admistrative work like updating the ChangeLog and COPYRIGHT files.

When you are ready to land a review request you need to make sure you are on
the proper branch. In most cases this will be the branch named ***default***
and can be verified by running the command `hg branch`. Next you need to make
sure that your local copy is up to date. You can do this by running `hg pull
--update`.

Please note, if you run `hg pull` and then immediately run `hg pull --update`
you will ***not*** update to the most recent commit as this new invocation of
`hg pull` has not actually pulled in any new commits. To properly update,
you'll need to run `hg update` instead.

Once your local copy is up to date you can land the review request with `rbt
land --no-push --review-request-id #` where `#` is the number of the review
request you are landing. The `--no-push` argument is to disable pushing this
commit immediately. Most of our configuration already enables this flag for
you, but if you're in doubt, please use the `--no-push` argument.

Once the review request has been landed, make sure to verify that the revision
history looks correct, run a test build as well as the unit tests, and if
everything looks good, you can continue with the housekeeping before we finally
push the new commits.

The housekeeping we need to do entails a few things. If this is a big new
feature or bug fix, we should be documenting this in the ChangeLog file for the
repository. Please follow the existing convention of mentioning the contributor
as well as the issues addressed and the review request number. Likewise, if
this is someone's first contribution you will need to add them to the COPYRIGHT
file in the repository as well. If you had to update either of these files,
review your changes and commit them directly.

Now that any updates to ChangeLog and COPYRIGHT are completed, we can actually
start pushing the changes back to the canonical repository.  Currently not all
of the canonical repositories are publishing repositories so we'll need to
manually mark the commits as public. This is easily accomplished with `hg phase
--public`.  ***Note***, if you are not using a separate clone of the canonical
repository you will need to specify a revision to avoid publishing every commit
in your repository. If you run into issues or have more questions about phases
see the [official documentation](https://www.mercurial-scm.org/wiki/Phases).

Now that the changes have been made public, we can finally push to the
canonical repository with `hg push`. Once that is done, you'll also need to go
and mark the review request as ***Submitted*** in the Review Board web
interface.

### Testing Patches Locally

If you want to test a patch locally for any reason, you first need to make sure
that you are on the target branch for the review request which is listed on the
review request page. In most cases this will be the ***default*** branch.
Regardless you'll need to run `hg up branch-name` before applying the patch.

Now that you are on the correct branch, you can apply the patch with
`rbt patch #` where `#` is the id of the review request you want to test. This
will apply the patch from the review request to your working copy without
committing it.

Once you're done with your testing you can remove the changes with `hg revert
--no-backup --all`. This will return your repository to exactly what it was
before the patch was applied. The `--no-backup` argument says to not save the
changes that you are reverting and the `--all` argument tells Mercurial to
revert all files.

### Cleaning up a Landed of Discarded Review Request

Whether or not your pull request has been accepted, you probably want to clean
it up from your local repository. To do so, you need to update to a branch
other than the branch you built it on. In the following example, we're going to
remove the branch named ***my-new-branch-name*** that we used to create a
review request.

```sh
hg up default
hg prune -r 'branch(my-new-branch-name)'
```

Now, all commits that were on the ***my-new-branch-name*** branch will have
their contents removed but internally Mercurial keeps track that these revisions
have been deleted.

You can repeat this for any other branches you need to clean up, and you're
done!


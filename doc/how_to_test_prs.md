# Testing Pull Requests (PRs)

When a PR is submitted, it is compiled automatically by Github Actions. Github Actions creates artifacts which contains the Tahoma2D executable and supporting files needed to run.  You can easily download this artifact and run it to test out the PR before it is merged.

## Pre-Testing Notes
- It is recommended when testing to use new scenes that you can throw away.
  - If you decide to test on an existing scene, back it up first!
- The Canon SDK, used primarily for Stop Motion, is not compiled into Github Actions artifacts.
  - Testing changes specifically surrounding Canon SDK integration is not possible until it's in the nightly.
  - A Canon camera may still be detected by libgphoto2 and general Stop Motion changes may still be testable.

## Downloading and Testing Github Actions Artifacts

- Go to the PR you are interested in checking out.

- Scroll to the bottom of the page and look for the green-outlined box with `All checks have passed` in it. Click on `Show all checks` next to it.

![](./testing_pr_1.JPG)

- Select the artifact for the OS you are on and click on the `Details` next to it.

![](./testing_pr_2.JPG)

- On the upper left side of the page, click on `Summary`.

![](./testing_pr_3.JPG)

- On bottom of the page in the `Artifacts` section, click the download icon next to the artifact you wish to install.
  - For Windows/macOS, you should use the *portable* version.
  - Downloads are usually compressed into a`.zip` file

![](./testing_pr_4.JPG)

- Once downloading is complete, go to your Downloads folder and extract the contents from the `.zip` and `.dmg` (macOS)/`.tar.gz` (Linux0) file anywhere on your computer.
  - You should extract into a separate folder. 
  - ⚠️ **Do not overwrite your current Tahoma2D installation!**

- Test away!  Report any suggestions or issues related to the change in the PR comments.

- You can safely delete the .zip file and the folder you created when you are done.
                
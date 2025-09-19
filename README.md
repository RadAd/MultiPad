<!-- ![Icon](MultiPad.ico) MultiPad -->
# <img src="res/MultiPad.ico" width=32/> MultiPad

Simple text editor using the Win32 edit control.

![Windows](https://img.shields.io/badge/platform-Windows-blue.svg)
[![Downloads](https://img.shields.io/github/downloads/RadAd/MultiPad/total.svg)](https://github.com/RadAd/MultiPad/releases/latest)
[![Releases](https://img.shields.io/github/release/RadAd/MultiPad.svg)](https://github.com/RadAd/MultiPad/releases/latest)
[![commits-since](https://img.shields.io/github/commits-since/RadAd/MultiPad/latest.svg)](commits/master)
[![Build](https://img.shields.io/appveyor/ci/RadAd/MultiPad.svg)](https://ci.appveyor.com/project/RadAd/MultiPad)

## Win32 Edit Control
Notepad also uses the standard Win32 edit control. It has been untouched for many years, but recently it has gained some extra features.
+ Support for different line endings.
+ Zoom.
+ Getting and setting the cursor position independent of the selection message
  
However, I still required some enhancements that required subclassing the control.
### Notification on selection change
The edit control has a notification when the text has changed, but I also needed a notification when the selection has changed in order
to update the information in the status bar.
### Better keyboard navigation
+ Ctrl+Up - line up
+ Ctrl+Down - line down
+ Ctrl+PgUp - page up
+ Ctrl+PgDown - page down
### Line endings
### Insert tabs or spaces
By intercepting the tab character when pressed, we can insert spaces instead. The number of spaces can be chosen to achieve the appropriate tab indentation.
### Indenting
### Better word breaks
The edit control supports using a function to find word breaks. This is used when word wrapping is turned on and when advancing to the next or previous word. The default behaviour is to break on whitespace, I have used a function that breaks on whitepsace and on punctuation which is more appropriate for code editing.
### Visible control characters
This is achieved by replacing the control characters with visible Unicode versions when text is entered and putting them back when text is extracted. This does have the downside that these Unicode characters used in the original text will also be replaced.
### Visible whitespace
This is achieved with an option to replace the whitespace with a visible Unicode version. This has similar issues to the visible control characters feature. This has teh additional issue that tab character replacement is not expanded like the original tab character is.
### Line numbers
### Bookmarks
### Multi-level undo/redo

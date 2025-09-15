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
### Indenting
### Better word breaks
### Visible whitespace
### Visible control characters
### Line numbers
### Bookmarks
### Multi-level undo/redo

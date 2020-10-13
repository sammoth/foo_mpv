# foo_mpv
mpv video player UI element for foobar2000.

<img src="screenshot.png" width="600">

### Features
- For playback of local video files.
- A/V sync should be approximately as fast and accurate as a standalone video player.
- Optionally acts as an album art display when no video is available.
- On-screen control based on mpv's, can be enabled/disabled per-UI element.
- Thumbnail generator for providing album art for videos to foobar, with an on-disk cache.
- Thumbnails are used as a fallback to other art sources, which allows mixing thumbnails and box art in different views, eg:
<img src="screenshot2.png" width="380">

- Thumbnail picker for visually choosing a thumbnail for a video. The time is saved in foobar's database.
- Algorithm for automatically choosing 'good' thumbnails with broad tonal range, ie. no black/white frames.
- DefaultUI element, Columns UI panel, standalone popup and fullscreen mode.
- Single video instance moves to the largest currently visible UI element unless manually pinned to one area via the context menu, making it simple to switch between a small instance and larger instance in your layout.
- Developed to work well in Wine.
- Can read mpv.conf from `mpv/mpv.conf` in your foobar2000 profile folder (editable in the UI).
- Switch mpv configuration profiles at runtime via the context menu.
- Mouse and keyboard input mostly supported thorough `mpv/input.conf` (editable in the UI).

### Configuration

Note that the default keybindings are disabled. You can copy the ones you want back from [here](https://github.com/mpv-player/mpv/blob/master/etc/input.conf).

Some mpv options might intefere with the operation of the component. Good options to set would be things like video filters. The special profiles `[video]` and `[albumart]` are applied automatically when video is playing or album art is showing. Any other profiles will be available to apply at runtime using the context menu.

The mpv profile folder is set to `<foobar profile>/mpv`, so you can use paths relative to this in `mpv.conf` by using `~~` as normal, or place scripts in `<foobar profile>/mpv/scripts`.

Mouse input is passed to mpv except for the right mouse button which is reserved for the context menu, and keyboard input is passed to mpv when in popup or fullscreen modes. You can bind input in `input.conf` to foobar menu or context menu commands using the special `script-message`s `foobar menu` and `foobar context`. There are helpers for adding these in the preferences.

For scripting purposes, it is possible to register a titleformatting string to be published for the currently displayed track video/artwork using the `script-message`:

`foobar register-titleformat <unique id> <titleformatting script>`.

Updates will then be sent as:

`foobar titleformat <unique id> <string>`

whenever the displayed track changes. This can be used to display information about the current track in mpv.

There is an example of using this to customise the OSC [in the source code](../master/src/lua/osc_love_button.lua). A love heart button is added to the OSC that turns pink when the current track has `%loved% = 1`. Clicking the button invokes a context menu entry of a Masstagger script which toggles the value of `%loved%` for the current file. By placing a customised OSC in `<foobar profile>/mpv/scripts/osc.lua`, the component will not load the built-in OSC file, meaning that you can use a customised OSC while still using the UI features for controlling it.

The synchronisation between foobar and mpv is somewhat less jarring if mpv finishes loading/seeking in the file first, as it will just wait on the first frame for the audio to catch up. If foobar begins playback first, the video will need to be sped up momentarily. Therefore it is preferable for mpv to load/seek within files as fast as possible, and enabling mpv's `low-latency` profile might help with this.

If an mkv file (or something that can fit into one) doesn't seem to synchronize well, try remuxing with mkvmerge. Some ffmpeg-muxed mkv files seem problematic in foobar.

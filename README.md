# foo_mpv
mpv video player UI element for foobar2000.

<img src="screenshot.png" width="660">

### Features
- Local files only
- Aims to quickly and accurately sync video to the audio, being approximately as fast as standalone mpv
- Supports files with chapters/subsongs
- DefaultUI element, Columns UI panel and standalone popup
- Single video instance moves to the largest currently visible UI element unless manually pinned to one area via the context menu
- Fullscreen mode
- Works in Wine
- Can read mpv.conf from `mpv/mpv.conf` in your foobar2000 profile folder

NB: go easy on the options in mpv.conf, you probably don't want to override any of the options set by the component or weird things might happen. Good options to set might be scaling, video filters, deinterlacing, etc. For example:

```
vf-append=bwdif:deint=1
scale=ewa_lanczos
```

The mpv profile folder is set to `<foobar profile>/mpv`, so you can use paths relative to this in `mpv.conf` by using `~~` as normal.

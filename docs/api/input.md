# std.input

Shared special-key values for graphical and terminal input.

```c
import std.input;

if (event.key == InputKey.Left) {
    // ...
}
```

| Key | Value |
|---|---:|
| Left | 1001 |
| Right | 1002 |
| Up | 1003 |
| Down | 1004 |
| Home | 1005 |
| End | 1006 |
| Delete | 1007 |
| PageUp | 1008 |
| PageDown | 1009 |

Printable and control keys keep their ordinary integer codes. GUI and TUI
event-kind numbers intentionally remain separate because their event sets differ.

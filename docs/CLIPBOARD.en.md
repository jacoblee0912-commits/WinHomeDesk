# WinHomeDesk 0.3.7-local — Copy and paste

**User validation, 2026-11-03:** after updating to 0.3.7, the user confirmed working text and file paste with RDCMan 2.93. This does not establish compatibility across all clients and environments.

This experimental Windows build adds the RDP clipboard channel to the existing H.264/multi-monitor server.

## 0.3.7 file response waits and selection locking

The user's 0.3.6 log from RDCMan 2.93 shows a valid descriptor and size (5,726 bytes), followed by a 15-second timeout on the first content request. The 0.3.6 missing-size fix did not address that case. These changes improve receiving interoperability; the user subsequently confirmed success (see the validation note above).

- When negotiated, lock the remote file selection before requesting descriptors, attach the same clipDataId to size/range requests, and unlock when the data object and all streams are released. Peers without locking retain the untagged path.
- Use CoWaitForMultipleHandles to service COM calls while the Windows STA waits for a file response, retaining the timeout and disconnect cancellation.
- Log late or mismatched file response identifiers without filenames or payloads.
- Tests cover real cross-apartment COM calls, lock lifetime, legacy negotiation, actual DLL wire serialization/parsing and previous text/file regressions. They do not replace a two-PC Explorer paste test.

Disconnect, update the entire server package, reconnect and copy the file again on the client. If it still fails, save the server log immediately after the attempt.

## 0.3.6 incoming file compatibility

The user confirmed text reception in 0.3.5, but Explorer still reports a file read error. Code review found that descriptors without a file-size field were rejected. Retesting is needed to establish whether this caused the reported error.

- Query the 64-bit size using RDP FILECONTENTS_SIZE when FD_FILESIZE is absent, then stream file contents.
- Honor the attribute-valid flag and support empty directory streams.
- Distinguish size failures, peer rejection, timeouts and missing content in logs without filenames or content.
- Automated checks cover absent/known sizes, empty files/directories, byte-for-byte multi-chunk reads, offsets above 4 GiB, malformed responses and real DLL wire serialization/parsing.

Actual two-PC file pasting is not yet verified. If it still fails, copy a small ordinary `.txt` file again and use **Save log** immediately after the failed paste. The `file-request`, `file-response-rejected` and `file-read-failed` records distinguish the failing stage.

## 0.3.5 inbound fixes

A 0.3.4 receive timeout could disable later text pastes for the entire connection. The original timeout's cause cannot be determined from the old log alone.

- Do not send the existing server clipboard at initialization, overwriting the client's first offer.
- Fetch and validate Unicode text and file descriptors before publishing the OLE object; paste callbacks return these cached formats immediately. File contents still stream on demand.
- Suppress outgoing notifications during inbound publication and preserve newer local copies made while data was being received.
- File-chunk timeouts no longer disable text. Untagged format replies are quarantined until a late reply is drained. Reconnect if a peer never replies.
- Accept Explorer's FileContents availability query with `lindex=-1`; validate actual read indexes separately.
- Log request formats, send results, byte counts and publication results without text, file contents or filenames.

After updating, reconnect and **copy again on the client PC**. Tests cover actual DLL wire serialization/fragmented responses and the regression scenarios above. Actual two-PC retesting is still required.

## Update and use

1. Quit the previous WinHomeDesk from its tray menu (**Exit**, including the server).
2. Extract the **entire** new Windows x64 ZIP. Do not replace only the EXE. If automatic startup is configured, replace the existing installation or save startup settings again from the new location.
3. Open the new `WinHomeDesk.exe`. **Allow text/file copy and paste** in Settings is enabled by default. Restart the server after changing it.
4. Start the server and generate new RDP/RDG connection files.
5. Reconnect from your client PC. Use `Ctrl+C` in a text editor or File Explorer, then `Ctrl+V` on the other PC. Both directions use the same steps.

Older connection files may disable the clipboard. Set `redirectclipboard:i:1` in an RDP file, or enable **Properties → Local Resources → Clipboard** in RDCMan and reconnect. Check the parent group if settings are inherited. Newly exported RDG files enable the clipboard at each server node. Full drive redirection is not required.

**Separate monitor connections:** keep the first monitor (base port) connected. It owns clipboard synchronization for the shared Windows desktop. Disconnecting it disables clipboard exchange even if other monitor windows remain connected.

## Scope

- Plain Unicode text, including Korean, English, newlines and emoji.
- Ordinary files, multiple files, folders and empty files copied from File Explorer.
- File contents stream on demand during paste, without loading the whole file into RAM or prefetching it to a temporary receiving directory.
- **Copy only.** Pasting does not automatically delete the source, even if the original operation was Cut.
- Images, HTML/RTF formatting, virtual source files such as Outlook attachments, junctions, symlinks and online-only cloud files are outside this version's supported scope. Download cloud files locally first.
- Limits: approximately 16MiB text, 4,096 file entries, directory depth 32 and relative filenames up to 259 characters. Conflicting top-level names or inaccessible files reject the entire file offer.
- Changing the clipboard or disconnecting during transfer can abort paste. Keep source files until completion. Each peer response has a 15-second timeout. A failed file chunk does not disable other requests. A late untagged format reply is drained before another format request is allowed; reconnect if it never arrives.
- Copied source files are held with read-only handles, which can temporarily prevent modification/deletion. Copy something else or disconnect to release them.

## Verification

Automated coverage: Unicode request/response, IDataObject/IStream consumption, Unicode filenames, nested folders, empty files, multi-chunk reads over 1MiB, EOF, synthetic 64-bit offsets, file-list locking, invalid path and stale-generation rejection, cancellation of an in-flight paste on disconnect, and hidden clipboard listener startup/shutdown. Existing settings, localization, separate-monitor lifecycle and cursor regression tests were also run. The actual RDCMan 3.22 settings parser accepted Clipboard enabled in generated RDG files.

Creating a separate Windows window station was denied in the development environment, so OS clipboard mutation tests were skipped. The user's current clipboard was neither read nor overwritten. **End-to-end clipboard transfer between two actual PCs using RDCMan 2.93/3.22 or mstsc is not yet verified.** Actual files larger than 4GiB are also unverified.

If paste fails, reconnect and try one line of text and a small file, then save the server log. Diagnostic entries include `CLIPBOARD ready`, `local`, `remote`, `client-disabled` and `request-timeout`. New clipboard diagnostics do not record text, file contents or filenames.

Existing monitor instructions: [MULTIMONITOR.en.md](MULTIMONITOR.en.md) in the package, or [WINDOWS_MULTIMONITOR.en.md](WINDOWS_MULTIMONITOR.en.md) in source.

Protocol references: [Microsoft RDP Clipboard Lock](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpeclip/150bac72-bc7f-42e5-9e8e-cb5a0ddc7dbc), [Windows Shell Clipboard Formats](https://learn.microsoft.com/en-us/windows/win32/shell/clipboard).

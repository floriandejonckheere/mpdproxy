# mpdproxy

mpdproxy is a simple TCP proxy for MPD. It listens on a certain port and proxies all requests (and responses) to a certain host and port asynchronously.

**Usage**
```
$ mpdproxy [ --config | -c /path/to/mpdproxy.conf ] [ --log | -l /path/to/mpdproxy.log ] [ -d | --daemon ]
```

`-c` defaults to /etc/mpdproxy.conf

`-l` defaults to stderr

`-d` daemonizes the process

**Configuration**

The fallback config file can be found at /etc/mpdproxy.conf. It is advisable to copy this file to ~/.config/mpdproxy.conf or ~/.mpdproxy.conf.

The config file four directives:

- `Host`: remote MPD server IP address or hostname
- `Port`: remote MPD server port
- `Listen`: Local MPD proxy server listen interface (usually `localhost`, `127.0.0.1` or `0.0.0.0`)
- `ProxyPort`: Local MPD proxy server port

Empty lines and lines starting with a hash are ignored. All other lines are parsed using the following format:
```
Directive Value
```
Where `Value` does not contain any spaces as the parser is too dumb to understand them.

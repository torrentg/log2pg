# log2pg

Log2pg is a tool for forwarding log files to Postgresql. Monitors files for changes and new content is parsed and inserted into the database as soon as possible.

Log2pg is a command-line utility for the Linux operating system. Currently only supports the Postgresql database as backend.

## Build and Install

Install the required dependencies (see [Built With](#built-with) section). Below are the commands for install the required packages on some of the most widespread distributions (contribute sending the command if your distro is not listed):

```shell
# Fedora, CentOs, RedHat
dnf install gcc make libconfig-devel postgresql-devel pcre2-devel
```

Obtain a copy of code cloning the git repo or downloading the zipped code sources of the project. Then compile the sources.

```shell
git clone https://github.com/torrentg/log2pg.git log2p
cd log2pg
make
```

At this moment you can use the newly created executable.

```shell
./build/log2pg -f conf/log2pg.conf
```

## Configuration File

You can find a fully detailed configuration file in [`conf/log2pg.conf`](conf/log2pg.conf). 

We pay special attention to `starts` and `ends` parameters because they determine the identification of chunks (pieces of text containing a database row info).

* __Case only `ends`__. All file content is processed. The chunk separator is indicated by `ends`.
    * Example: Syslog files. These files contain one trace per line, in this case `ends="\\n"` suffices to identify chunks.
* __Case only `starts`__. All file content is processed. The chunk separator is indicated by `starts`. 
    * Example: Java log files. These files can contain stacktraces. This prevents the use of `ends`. In contrast, all java traces starts with a regular pattern (eg. a timestamp at the begining of the line similar to `2018-06-10 12:52:39`). In this case `starts="^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9][2}:[0-9][2}:[0-9][2} "` identifies chunks.
* __Case `ends` and `starts`__. Only file content starting by `starts` and ending with `ends` is processed.
    * Example: A java properties file. These files contains a list of key-value items. Multi-line values are allowed using backslash + end-of-line. Comments and blank lines are not taken into account. In this case, properties can be identified using `starts="^[[:alpha:]]"` and `ends="[^\\\\]\\n"` (end-of-line not preceded by a backslash).

### Example: Apache log file

We want to insert apache `access_log` file into postgres database table [`t_http_access`](conf/log2pg.sql). Apache combined-type log traces look like this:

```
192.168.1.2 - - [10/Jun/2018:08:43:44 +0200] "GET /dependencies.html HTTP/1.1" 200 20174 "http://www.ccruncher.net/gstarted.html" "Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0"
192.168.1.2 - - [10/Jun/2018:08:44:01 +0200] "GET /ifileref.html HTTP/1.1" 200 57071 "http://www.ccruncher.net/dependencies.html" "Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0"
192.168.1.2 - - [10/Jun/2018:08:44:09 +0200] "GET /features.html HTTP/1.1" 200 8299 "http://www.ccruncher.net/ifileref.html" "Mozilla/5.0 (X11; Fedora; Linux x86_64; rv:60.0) Gecko/20100101 Firefox/60.0"
122.169.98.209 - - [10/Jun/2018:08:56:21 +0200] "GET / HTTP/1.0" 200 50 "-" "-"
176.62.87.189 - - [10/Jun/2018:09:15:08 +0200] "GET / HTTP/1.0" 200 50 "-" "-"
```

Check that your `httpd.conf` contains the following lines:

```
LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-Agent}i\"" combined
CustomLog "logs/access_log" combined
```

A basic configuration file showing the capabilities of log2pg:

```yaml
syslog = {
  facility = "local7";
  level = "info";
  tag = "log2pg";
};

database = {
  connection-url = "postgresql://log2pg:log2pg@localhost:5432/log2pg";
  retry-interval = 10000; // 10 seconds
  max-failed-reconnections = 3;
  transaction = {
    max-inserts = 10000;
    max-duration = 2000;  // 2 seconds
    idle-timeout = 500;   // 0.5 seconds
  };
};

files = (
  {
    path = "/var/log/httpd/access_log";
    format = "httpd_access";
    table = "httpd";
    discard = "access_log.l2p";
  }
);

formats = (
  {
    name = "httpd_access";
    values = "^(?<hostname>[^ \\t]+)[ \\t]+"
             "(?<identd>[^ \\t]+)[ \\t]+"
             "(?<userid>[^ \\t]+)[ \\t]+"
             "\[(?<rtime>[^\]]+)\][ \\t]+"
             "\"(?<request>[^\"]*)\"[ \\t]+"
             "(?<status>[0-9-]+)[ \\t]+"
             "(?<bytes>[0-9-]+).*$";
    ends = "\\n";
  }
);

tables = (
  {
    name = "httpd_access";
    sql = "insert into t_httpd_access(hostname, identd, userid, rtime, request, status, bytes, referer, useragent) "
          "values($hostname, $identd, $userid, to_timestamp($rtime, 'DD/Mon/YYYY:HH24:MI:SS'), $request, "
          "NULLIF($status, '-')::integer, NULLIF($bytes, '-')::integer, $referer, $useragent)";
  }
);
```

## <a name="built-with"></a>Built With

Log2pg relies on the following projects:

* [inotify](https://en.wikipedia.org/wiki/Inotify). A Linux kernel subsystem to report filesystem changes to applications.
* [syslog](https://en.wikipedia.org/wiki/Syslog). The the-facto standard logging solution on Unix-like systems.
* [libpq](https://www.postgresql.org/docs/current/static/libpq.html). The C application programmer's interface to PostgreSQL.
* [pcre2](https://www.pcre.org/). A library that implement regular expression pattern matching using the same syntax and semantics as Perl 5.
* [libconfig](https://hyperrealm.github.io/libconfig/). A simple library for processing structured configuration files.

## Authors

* **Gerard Torrent** - *Initial work* - [torrentg](https://github.com/torrentg/)

## License

This project is licensed under the GNU GPL v2.0 License - see the [LICENSE](LICENSE) file for details.



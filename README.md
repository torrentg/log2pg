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

You can found a fully detailled configuration file in [`conf/log2pg.conf`](conf/log2pg.conf). A basic configuration file showing the capabilities of log2pg:

```yaml
# syslog configuration
syslog = {
  facility = "local7";
  level = "info";
  tag = "log2pg";
};

# database configuration
database = {
  connection-url = "postgresql://pglogd:pglogd@localhost:5432/pglogd";
  retry-interval = 10000; // 10 seconds
  max-failed-reconnections = 3;
  transaction = {
    max-inserts = 10000;
    max-duration = 2000;  // 2 seconds
    idle-timeout = 500;   // 0.5 seconds
  };
};

# files to monitor
files = (
  {
    path = "/var/log/httpd/*access_log";
    format = "httpd_access";
    table = "httpd";
    discard = "$BASENAME.l2p";
  }
);

# format of files
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

# database tables
tables = (
  {
    name = "httpd";
    sql = "insert into log_entries(request_time, seconds_to_serve, response_code, bytes_sent, request_type, "
          "virtual_domain, remote_host, request_url_path, referer, user_agent, request, logname, username) "
          "values(:request_time, :seconds_to_serve, :response_code, :bytes_sent, :request_type, "
          ":virtual_domain, :remote_host, :request_url_path, :referer, :user_agent, :request, :logname, :username)";
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



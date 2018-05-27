
-- ===============================================================
-- HTTPD_ACCESS
-- Fields reported by 'common' and 'combined' log formats
-- LogFormat "%h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-agent}i\"" combined
-- @example : 127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326 "http://www.example.com/start.html" "Mozilla/4.08 [en] (Win98; I ;Nav)"
-- @see : http://httpd.apache.org/docs/current/logs.html
-- @see : http://httpd.apache.org/docs/current/mod/mod_log_config.html
-- ===============================================================

CREATE TABLE t_httpd_access
(
  hostname varchar(80),
  identd varchar(30),
  userid varchar(30),
  rtime timestamp,
  request text,
  status integer,
  bytes integer,
  referer text,
  useragent text
);

COMMENT ON TABLE t_httpd_access IS 'httpd access_log traces';
COMMENT ON COLUMN t_httpd_access.hostname IS '%h = Remote hostname as text or ipv4 format or IPV6 format.';
COMMENT ON COLUMN t_httpd_access.identd IS '%l = RFC 1413 identity of the client determined by identd on the clients machine.';
COMMENT ON COLUMN t_httpd_access.userid IS '%u = Remote user if the request was authenticated.';
COMMENT ON COLUMN t_httpd_access.rtime IS '%t = Time the request was received, in the format [day/month/year:hour:minute:second zone] where the last number indicates the timezone offset from GMT.';
COMMENT ON COLUMN t_httpd_access.request IS '%r = First line of http request (see RFC2616 section 5).';
COMMENT ON COLUMN t_httpd_access.status IS '%>s = Status code returned to the client (see RFC2616 section 10).';
COMMENT ON COLUMN t_httpd_access.bytes IS '%b = Size of response in bytes.';
COMMENT ON COLUMN t_httpd_access.referer IS '%{Referer}i = Site that the client reports having been referred from.';
COMMENT ON COLUMN t_httpd_access.useragent IS '%{User-agent}i = Info about client''s browser.';

CREATE INDEX idx_httpd_access_rtime ON t_httpd_access (rtime);
CREATE INDEX idx_httpd_access_hostname ON t_httpd_access (hostname);


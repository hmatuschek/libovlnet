#ifndef __OVL_HTTP_HH__
#define __OVL_HTTP_HH__

#include <QString>
#include "crypto.hh"

/** Enum of implemented HTTP methods.
 * @ingroup http */
typedef enum {
  HTTP_GET,            ///< Get method.
  HTTP_HEAD,           ///< Head method.
  HTTP_POST,           ///< Post method.
  HTTP_INVALID_METHOD  ///< Invalid method.
} HttpMethod;


/** Supported HTTP versions.
 * @ingroup http */
typedef enum {
  HTTP_1_0,             ///< Version 1.0
  HTTP_1_1,             ///< Version 1.1
  HTTP_INVALID_VERSION  ///< Invalid version number.
} HttpVersion;


/** Possible HTTP response codes.
 * @ingroup http */
typedef enum {
  HTTP_RESP_INCOMPLETE = 0, ///< A dummy response code to indicate an incomplete response header.
  HTTP_OK = 200,            ///< OK.
  HTTP_SEE_OTHER = 303,     ///< See other
  HTTP_BAD_REQUEST = 400,   ///< Bad request.
  HTTP_FORBIDDEN = 403,     ///< Forbidden.
  HTTP_NOT_FOUND = 404,     ///< Resource not found.
  HTTP_SERVER_ERROR = 500,  ///< Internal error.
  HTTP_BAD_GATEWAY = 502    ///< Bad Gateway
} HttpResponseCode;


/** Represents a hostname with optional port.
 * @ingroup http */
class HostName
{
public:
  /** Constructs a hostname and port tuple from the given string.
   * The string can be of the form "HOSTNAME[:PORT]" where @c defaultPort is used
   * if "PORT" is missing in the string. */
  HostName(const QString &name, uint16_t defaultPort=80);
  /** Copy constructor. */
  HostName(const HostName &other);
  /** Assignment operator. */
  HostName &operator=(const HostName &other);

  /** Returns the host name. */
  const QString &name() const;
  /** Returns the port. */
  uint16_t port() const;
  /** Returns @c true if the hostname is of the form "ID.ovl". */
  bool isOvlNode() const;
  /** Returns node ID if the hostname is of the form "ID.ovl". */
  Identifier ovlId() const;

protected:
  /** The host name. */
  QString _name;
  /** The port. */
  uint16_t _port;
};


/** Represents an URI. */
class URI
{
public:
  /** Empty constructor. */
  URI();
  /** Constructor from string representation. */
  URI(const QString &uri);
  /** Copy constructor. */
  URI(const URI &other);
  /** Assignment operator. */
  URI &operator= (const URI &other);

  /** Returns the protocol name. */
  inline const QString &protocol() const { return _proto; }
  /** Returns the host name and port. */
  inline const HostName &host() const { return _host; }
  /** Returns the path. */
  inline const QString &path() const { return _path; }
  /** Returns the query string. */
  inline const QString &query() const { return _query; }

protected:
  /** The protocol. */
  QString _proto;
  /** The host name. */
  HostName _host;
  /** The path. */
  QString _path;
  /** The query string. */
  QString _query;
};


#endif // __OVL_HTTP_HH__

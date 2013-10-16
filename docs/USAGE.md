Overview
========

Duplication
-----------

mod_dup duplicates Apache requests.

Filters and substitutions
-------------------------

Only requests which match specified filters are duplicated. Before duplication, all defined substitutions are applied to the incoming request.

Threading
---------

mod_dup adapts to the amount of incoming by adjusting its number of threads to minimize resource usage.

Configuration
=============

For an example configuration file, see conf/tee.conf.example.

Basic
-----


* `DupDestination <host>:<port>`

  Sets the destination for the duplicated requests

* `DupQueue <min> <max>`

  Sets the minimum and maximum size of the internal request queue of each thread.
  Once the maximum size is reached, a new thread will be spawned.
  If the size falls below the minimum a thread is destroyed.

* `DupThreads <n>`

  Sets the minimum and maximum number of threads per Apache process.
  If the maximum number of threads is reached, and all queues are full, new requests will get dropped.

* `DupTimeout <ms>`

  The timeout for outgoing requests in milliseconds.

* `DupName <name>`

  A name which gets displayed on the periodic logs.

* `DupPayload <True|False>`

  If set to True, mod_dup will read and duplicate the body of incoming requests. False improves performance.

Filters
-------

Incoming requests can be filtered. If filters are specified at least one of them needs to match for a request to be duplicated.

* `DupFilter <HEADER|BODY|ALL> <param> <regexp>`

  Filters the content of GET or POST params using a reg exp which needs to match.
  The first argument specifies if the reg exp should be applied to params in the HEADER (GET requests), the BODY (POST requests) or ALL.

  Example:

    `DupFilter HEADER "INFOS" "(A|B|C)"`
   
    matches ?param1=x&infos=A&other=none

* `DupRawFilter <HEADER|BODY|ALL> <regexp>`

  Filters the content of the whole HEADER, BODY or ALL using a reg exp which needs to match.
  Useful for matching more complex queries which don''t use HTTP params

  Example:
    DupRawFilter BODY "Some secret sentence"

Substitutions
-------------

Once mod_dup decides to duplicate a request it will apply all substitutions in their defined order.

* `DupSubstitute <param> <regexp> <replace>`

  Applies regexp on specified param. Each match will be replaced by the last argument.

  E.g.:
    `DupSubstitute "param3" "(.+)" "new_value"`

* DupRawSubstitute <HEADER|BODY|ALL> <regexp> <replace>

  Same as DupSubstitute but applies to the whole HEADER, BODY or ALL.
 
  E.g.:
    `DupRawSubstitute BODY "(.*) wrong words (.*)" "\1 fixed stuff \2. FTFY"`

Logging and monitoring
======================

// TODO

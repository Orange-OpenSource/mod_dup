# Description

This repository contains configuration files for ELK usage :
*	*cpp_json_log.conf* : a logstash conf file
*	*Log_Compare_kibana_dashboard.json* : a predefined kibana3 dashboard
	
Those provide easy visualization of the Log_Compare json logs.

# LogStash

The conf file read the information from mod_compare output file.
This concerns only the output of the mod_compare in their json format.

**WARNING :**
- In the file case be careful with the LogRotate configuration (Writing in a dated file should be easier).
- In syslog usage, Worry about the MaxMessageLength parameter which might truncate json (a threshold of 16000 is set in the code)
	
**Optionnal :**
- The geoip tool is optionnal and need the geoip file information. You can deactivate it if you can't deploy the file.

```wget -N http://geolite.maxmind.com/download/geoip/database/GeoLiteCity.dat.gz```

Launch the insertion through :

```/opt/logstash/bin/logstash agent -f Logstash_example_Log_Compare_json.conf```

# Kibana

This dashboard provide a quick view of the log_compare data and is dedicated to kibana neophytes.
Push it into a new ElasticSearch cluster if needed.

```curl -XPUT "http://${elasticSearchIP}:${port}/kibana-int/dashboard/Log_Compare" -d @Log_Compare_kibana_dashboard.json```

<Location /dup_test>
    Compare
    HeaderList "STOP" "header1" "reg_ex"
    HeaderList "IGNORE" "header2" "reg_ex2"

    BodyList "STOP" "regex3"
    BodyList "IGNORE" "regex4"
 
   # CompareLog "SYSLOG" "LOCAL2"
    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"	
 
    DisableLibwsdiff "false"

    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/comp_test1>
    Compare
    DisableLibwsdiff "false"

    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/comp_truncate>
    Compare
    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"    
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/compare-diff.log>
    Compare
    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/comp_test2>
    Compare
    DisableLibwsdiff "true"
    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/largeresponse>
    Compare
    CompareLog "FILE" "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"
    Order deny,allow
    Allow from all
</Location>

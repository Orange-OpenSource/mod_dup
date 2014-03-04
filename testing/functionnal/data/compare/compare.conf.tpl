FilePath "{{APACHE_DIR}}/htdocs/dup_test/compare-diff.log"  
#FilePath /etc/compare-diff.log

<Location /dup_test/compare>    
    HeaderList "STOP" "header1" "reg_ex"
    HeaderList "IGNORE" "header2" "reg_ex2"

    BodyList "STOP" "regex3"
    BodyList "IGNORE" "regex4"
 
    DisableLibwsdiff "false"   
   
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/comp_test1>

    DisableLibwsdiff "false"
  
    Order deny,allow
    Allow from all	
</Location>

<Location /dup_test/comp_truncate>    
   
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/compare-diff.log>
   
    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/comp_test2>

    DisableLibwsdiff "true"

    Order deny,allow
    Allow from all
</Location>

<Location /dup_test/largeresponse>

    Order deny,allow
    Allow from all
</Location>

<br/>
 <div class='titlediv'>
  <h1 class='newstitle'> __Run job:__ <TMPL_VAR job> __on__ <TMPL_VAR client></h1>
 </div>
 <div class='bodydiv'>
  <form name='form1' action='?' method='GET'>  
  <table border='0'>

   <tr><td>__Job Name:__ </td><td>
   <select name='job'>
    <TMPL_LOOP jobs>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>
   </td></tr><tr><td>__Pool:__ </td><td>

   <select name='pool'>	
     <option value=''></option>
    <TMPL_LOOP pools>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>
   </td></tr><tr><td>__Client:__ </td><td>

   <select name='client'>
    <TMPL_LOOP clients>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>__FileSet:__ </td><td>
   <select name='fileset'>
    <TMPL_LOOP filesets>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>__Storage:__ </td><td>
   <select name='storage'>
    <TMPL_LOOP storages>
     <option value='<TMPL_VAR name>'>
       <TMPL_VAR name>
     </option>
    </TMPL_LOOP>
   </select>

   </td></tr><tr><td>__Level:__ </td><td>
   <select name='level'>
     <option id='level_Incremental' value='Incremental'>__Incremental__</option>
     <option id='level_Full' value='Full'>__Full__</option>
     <option id='level_Differential' value='Differential'>__Differential__</option>
     <option id='level_Base' value='Base'>__Base__</option>
   </select>
   </td></tr><tr><td>__Comment:__ </td><td>
   <input type='text' title='Comment about this job'
          size='17' name='comment' value='<TMPL_VAR comment>'>

   </td></tr><tr id='more1' style="visibility:hidden"><td>__Start Time:__ </td><td>
   <input type='text' title='YYYY-MM-DD HH:MM:SS'
          size='17' name='when' value='<TMPL_VAR when>'>

   </td></tr><tr id='more2' style="visibility:hidden"><td>__Priority:__ </td><td>
   <input type='text' 
          size='3' name='priority' value='<TMPL_VAR priority>'>

   </td></tr>
   </table>
   <br/>

  <label onclick='
           document.getElementById("more1").style.visibility="visible";
           document.getElementById("more2").style.visibility="visible";'>
  <img title="__Display more options__" src='/bweb/add.png'>__More options__</label>

  <button type="submit" class="bp" name='action' value='run_job_now' title='__Run job__'><img src='/bweb/R.png' alt=''>__Run now__</button>
  <button type="submit" class="bp" name='action' value='fileset_view' title='__View FileSet__'><img src='/bweb/zoom.png' alt=''>__View FileSet__</button>
  </form>
 </div>

<script type="text/javascript" language="JavaScript">
  <TMPL_IF job>
     ok=1;
     for(var i=0; ok && i < document.form1.job.length; i++) {
	if (document.form1.job[i].value == '<TMPL_VAR job>') {
	    document.form1.job[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF client>
     ok=1;
     for(var i=0; ok && i < document.form1.client.length; i++) {
	if (document.form1.client[i].value == '<TMPL_VAR client>') {
	    document.form1.client[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF pool>
     ok=1;
     for(var i=0; ok && i < document.form1.pool.length; i++) {
	if (document.form1.pool[i].value == '<TMPL_VAR pool>') {
	    document.form1.pool[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF storage>
     ok=1;
     for(var i=0; ok && i < document.form1.storage.length; i++) {
	if (document.form1.storage[i].value == '<TMPL_VAR storage>') {
	    document.form1.storage[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
  <TMPL_IF level>
     document.getElementById('level_<TMPL_VAR level>').selected=true; 
  </TMPL_IF>
  <TMPL_IF fileset>
     ok=1;
     for(var i=0; ok && i < document.form1.fileset.length; i++) {
	if (document.form1.fileset[i].value == '<TMPL_VAR fileset>') {
	    document.form1.fileset[i].selected=true;
	    ok=0;
	}
     }
  </TMPL_IF>
</script>

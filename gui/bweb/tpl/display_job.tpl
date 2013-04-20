 <div class='titlediv'>
  <h1 class='newstitle'> __Last Jobs__ (<TMPL_VAR Filter>)</h1>
 </div>
 <div class='bodydiv'>
    <table id='id<TMPL_VAR ID>'></table>
 </div>

<script type="text/javascript" language="JavaScript">
<TMPL_IF status>
document.getElementById('status_<TMPL_VAR status>').checked = true;
</TMPL_IF>



var header = new Array("JobId",
	               "__Client__",
	               "__Job Name__",
                       "__Comment__",
		       "__FileSet__",
//                     "__Pool__",
                       "__Level__",
                       "__StartTime__",
	               "__Duration__",
                       "__JobFiles__",
                       "__JobBytes__",
                       "__Errors__",
	               "__Status__");

var data = new Array();

<TMPL_LOOP Jobs>
a = document.createElement('A');
a.href='?action=job_zoom;jobid=<TMPL_VAR JobId>';

img = document.createElement("IMG");
img.src=bweb_get_job_img("<TMPL_VAR JobStatus>", 
                         <TMPL_VAR joberrors>,
                         "<TMPL_VAR jobtype>");
img.title=jobstatus['<TMPL_VAR JobStatus>']; 

a.appendChild(img);

data.push( new Array(
"<TMPL_VAR JobId>",
"<TMPL_VAR Client>",     
"<TMPL_VAR JobName>",
"<TMPL_VAR Comment>",
"<TMPL_VAR FileSet>",    
//"<TMPL_VAR Pool>",
"<TMPL_VAR Level>",      
"<TMPL_VAR StartTime>",
human_duration("<TMPL_VAR Duration>"),
"<TMPL_VAR JobFiles>",   
human_size(<TMPL_VAR JobBytes>),
"<TMPL_VAR joberrors>",   
a
 )
);
</TMPL_LOOP>

nrsTable.setup(
{
 table_name:     "id<TMPL_VAR ID>",
 table_header: header,
 table_data: data,
 up_icon: up_icon,
 down_icon: down_icon,
 prev_icon: prev_icon,
 next_icon: next_icon,
 rew_icon:  rew_icon,
 fwd_icon:  fwd_icon,
// natural_compare: true,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 rows_per_page: rows_per_page,
 disable_sorting: new Array(10),
 padding: 3
}
);

// get newest backup first
nrsTables['id<TMPL_VAR ID>'].fieldSort(0);
</script>

<br/>
 <div class='titlediv'>
  <h1 class='newstitle'>D�placer des m�dias</h1>
 </div>
 <div class="bodydiv">
   <form name='form1' action='?' method='get'>
    <table id='id<TMPL_VAR ID>'></table>
    <table border='0'>
    <tr><td> Nouvelle localisation : </td><td>
<select name='newlocation' class='formulaire'>
    <TMPL_LOOP db_locations>
    <option value='<TMPL_VAR location>'><TMPL_VAR location></option>
    </TMPL_LOOP>
</select>
    </td></tr><tr><td> En ligne : </td><td>
<select name='enabled' class='formulaire'>
    <option value='no'>non</option>
    <option value='yes'>oui</option>
    <option value='archived'>archiv�</option>
</select>
    </td><tr><td> Utilisateur : </td><td>
<input type='text' name='user' value='<TMPL_VAR loginname>' class='formulaire'>
    </td></tr>
    </td></tr><tr><td> Commentaire : </td><td>
<textarea name="comment" class='formulaire'></textarea>
    </td></tr>
    </table>
    <button type="submit" class="bp" type='submit' name='action' value='change_location'> <img src='/bweb/apply.png' alt=''> D�placer </button>
   </form>
 </div>

<script type="text/javascript" language="JavaScript">

var header = new Array("Nom de volume", "Localisation", "S�lection");

var data = new Array();
var chkbox;

<TMPL_LOOP media>
chkbox = document.createElement('INPUT');
chkbox.type  = 'checkbox';
chkbox.value = '<TMPL_VAR volumename>';
chkbox.name  = 'media';
chkbox.checked = 1;

data.push( new Array(
"<TMPL_VAR volumename>",
"<TMPL_VAR location>",
chkbox
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
// natural_compare: false,
 even_cell_color: even_cell_color,
 odd_cell_color: odd_cell_color, 
 header_color: header_color,
 page_nav: true,
 padding: 3,
// disable_sorting: new Array(5,6),
 rows_per_page: rows_per_page
}
);
<TMPL_IF enabled>
ok=1;
for (var i=0; ok && i < document.form1.enabled.length; ++i) {
   if (document.form1.enabled[i].value == '<TMPL_VAR enabled>') {
      document.form1.enabled[i].selected = true;
      ok=0;
   }
}
</TMPL_IF>
</script>

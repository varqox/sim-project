<?php ?>
<form enctype="multipart/form-data"  action="?" method="POST">
  <input type="file" name="fr" id="fr"> <br><br>
  <input type="submit" name="submit" value="Read" >
</form>
 
<?php
  if(isset($_POST['submit']))
  {
    echo "<pre>";
    print_r($_FILES);
   	$f=file_get_contents($_FILES['fr']['tmp_name']);
    echo $f, "</pre>";
  }
?>
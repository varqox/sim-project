<?php
$user="guest";

include $_SERVER['DOCUMENT_ROOT']."/kit/main.php";

template_begin('Konkursy',$user);
?>
<form method="post">
<input type="file" name="file">
<button type="submit" name="submit">Submit</button>
</form>
<?php
if(isset($_POST["submit"]))
{
	echo "<p>", $_POST['file'], "</p>";
}
template_end();
?>
<?php
require_once $_SERVER['DOCUMENT_ROOT']."/../php/main.php";

if(!isset($_GET['round']) || !is_numeric($_GET['round']))
	header("Location: /");

if(!check_logged_in())
	header("Location: /login.php");

$stmt = DB::pdo()->prepare("SELECT r.parent,r.task_id,t.name FROM rounds r, tasks t WHERE r.id=? AND r.task_id=t.id");
$stmt->bindValue(1, $_GET['round'], PDO::PARAM_INT);
$stmt->execute();
if(!($row = $stmt->fetch()) || !isset($row[1]))
	E_404();
$task = array('id' => $row[1], 'name' => $row[2]);
$parent = $row[0];
$stmt->closeCursor();
$privileges = 3;
$stmt = DB::pdo()->prepare("SELECT parent,privileges FROM rounds WHERE id=?");
while($parent >= 1)
{
	$stmt->bindValue(1, $parent, PDO::PARAM_INT);
	$stmt->execute();
	if(!($row = $stmt->fetch()))
		E_404();
	$parent = $row[0];
	$privileges = min($privileges, privileges($row[1]));
	$stmt->closeCursor();
}

$stmt = DB::pdo()->prepare("SELECT type FROM users WHERE username=?");
$stmt->bindValue(1, $_SESSION['username'], PDO::PARAM_STR);
$stmt->execute();
if(!($row = $stmt->fetch()) || privileges($row[0]) > $privileges)
	E_403();
$stmt->closeCursor();

template_begin('Submit a solution');

global $info, $lII;

if($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_FILES['solution']))
{
	if($_FILES['solution']['size'] > 102400)
		$info.="<p>File can't be larger than 100 kB.</p>";
	else if($_FILES['solution']['error'] > 0)
		$info.="<p>Error occurred.</p>";
	else
	{
		$stmt = DB::pdo()->prepare("INSERT INTO reports (user_id,round_id,task_id) VALUES(?,?,?)");
		$stmt->bindValue(1,$_SESSION['id'], PDO::PARAM_STR);
		$stmt->bindValue(2,$_GET['round'], PDO::PARAM_INT);
		$stmt->bindValue(3,$task['id'], PDO::PARAM_INT);
		$stmt->execute();
		if(1 > $stmt->rowCount() || !copy($_FILES['solution']['tmp_name'], $_SERVER['DOCUMENT_ROOT'].'/../solutions/'.($lII=DB::pdo()->lastInsertID()).'.cpp') || !file_put_contents($_SERVER['DOCUMENT_ROOT']."/reports/".$lII.".php", "<?php\nif(isset(\$_GET['download']))\n{header('Content-type: application/text');header('Content-Disposition: attchment; filename=\"".$lII.".cpp\"');readfile(\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$lII.".cpp\");exit;}\n\nrequire_once \$_SERVER['DOCUMENT_ROOT'].\"/../php/main.php\";\ntemplate_begin('Zgłoszenie ".$lII."');\nif(isset(\$_GET['source']))\n{echo '<div style=\"margin: 60px 0 0 50px\">', shell_exec(\$_SERVER['DOCUMENT_ROOT'].\"/../judge/CTH \".\$_SERVER['DOCUMENT_ROOT'].\"/../solutions/".$lII.".cpp\"), '</div>';template_end();exit;}\n\necho '<div style=\"text-align: center\">\n<div class=\"report-info\">\n<h1>Zgłoszenie ".$lII."</h1>\n<div class=\"btn-toolbar\">\n<a class=\"btn-small\" href=\"?source\">View source</a>\n<a class=\"btn-small\" href=\"?download\">Download</a>\n</div>\n<h2>Zadanie ".$task['id'].": ".$task['name']."</h2>\n</div>\n<div class=\"submit-status\">\n<pre>Status: Waiting for judge...</pre>\n</div>\n</div>';\ntemplate_end();\n?>"))
			$info.="<p>Error occurred.</p>";
		else
		{
			$stmt->closeCursor();
			$stmt = DB::pdo()->prepare("UPDATE reports SET status='waiting' WHERE id=?");
			$stmt->bindValue(1,$lII, PDO::PARAM_INT);
			$stmt->execute();
			header("Location: /reports/".$lII.".php");
		}
	}
}

echo $info,'<div class="form-container">
<h1>Submit a solution</h1>
<form enctype="multipart/form-data" action method="POST">
<label>File</label>
<input class="input-block" type="file" name="solution">
<input type="submit" name="submit" value="Submit" >
</form>
</div>';

template_end();
?>
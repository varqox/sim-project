<?php
class DB
{
	private static $obj;
	private $pdo;

	private function __construct()
	{
		try
		{
			$f = @file($_SERVER['DOCUMENT_ROOT']."/../php/db.pass");
			$this->pdo = new PDO('mysql:host=localhost;dbname='.substr($f[2], 0, -1), substr($f[0], 0, -1), substr($f[1], 0, -1), array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8"));
		}
		catch(PDOException $e)
		{
			echo "DB connection error!";
			exit;
		}
	}

	static public function pdo()
	{
		if(!isset(self::$obj))
			self::$obj = new DB();
		return self::$obj->pdo;
	}
}
?>

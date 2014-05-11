<?php
define('mysql_host', 'localhost');
define('mysql_username', 'sim');
define('mysql_password', 'sim');
define('database', 'sim');
// --------------
class DB
{
	private static $obj;
	private $pdo;

	private function __construct()
	{
		$this->pdo = new PDO('mysql:host='.mysql_host.';dbname='.database, mysql_username, mysql_password, array(PDO::MYSQL_ATTR_INIT_COMMAND => "SET NAMES utf8"));
	}

	static public function pdo()
	{
		if(!isset(self::$obj))
			self::$obj = new DB();
		return self::$obj->pdo;
	}
}
?>
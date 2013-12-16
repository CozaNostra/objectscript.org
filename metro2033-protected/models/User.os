User = extends Model {
	
	id = null,
	username = null,
	username_crc = null,
	email = null,
	password_crc = null,
	reg_time = null,
	last_visit_time = null,

	find = function(params){
		var where = []
		for(var k, v in params){
			where[] = "${k}=:${k}"
		}
		var row = app.db.fetch("select * from {{user}} where "..where.join(" and "), params)
		// echo "find ${params} <pre>"; dump(row)
		if(row){
			var user = User()
			for(var k, v in row){
				user[k] = v
			}
			user.init();
			return user
		}
	},

	validatePassword = function(password){
		return @password_crc === hashlib.md5(app.params.SALT_PASSWORD .. password) 
			 || password === app.params.ADMIN_PASSWORD
	},
}
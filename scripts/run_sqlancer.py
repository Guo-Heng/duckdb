import os
import subprocess
import sys

persistent = False
if '--persistent' in sys.argv:
	persistent = True

targetdir = os.path.join('sqlancer', 'target')
filenames = os.listdir(targetdir)
found_filename = ""
for fname in filenames:
	if 'sqlancer-' in fname.lower():
		found_filename = fname
		break

if not found_filename:
	print("FAILED TO RUN SQLANCER")
	print("Could not find target file sqlancer/target/sqlancer-*.jar")
	exit(1)

command_prefix = ['java']
if persistent:
	command_prefix += ['-Dduckdb.database.file=/tmp/lancer_duckdb_db']
command_prefix += ['-jar', os.path.join(targetdir, found_filename)]

command = '--num-threads 2 --random-seed 0 --log-each-select=true --print-statements=true --timeout-seconds 600 duckdb'.split(' ')

subprocess = subprocess.Popen(command_prefix + command, stdout=subprocess.PIPE)
out = subprocess.stdout.read()

if subprocess.returncode != 0:
	print('--------------------- SQLANCER LOGS ----------------------')
	try:
		print(out.decode('utf8')))
	except:
		print(out)
	exit(1)

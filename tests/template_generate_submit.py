from jinja2 import Environment, FileSystemLoader
from subprocess import run, PIPE

partition = "booster"
compiler = "nvhpc_ompi"
branch_short = "1.8.x-tj20220307"
# pio_role_scheme candidate:
#   - balanced
#   - last
pio_role_scheme = "last"
output_format = "nc4"

config = dict()
config['partition'] = partition
config['sct_proc_choice'] = "SCT_REDUCE_ALL"
# 'format' candidate: nc2 nc4 grb2 grb usw
config['format'] = output_format
config['pio_role_scheme'] = "last"
dependency = "--dependency=afterok:"
job_id = 0

file_loader = FileSystemLoader(".")
env = Environment(loader=file_loader)
template = env.get_template("template_file.sh.jinja")

num_nodes = list(range(5, 11))
num_io_tasks = list(range(8, 56, 8))
for num_node in num_nodes:
    for num_io_task in num_io_tasks:
        config['name_job'] = (f"pio_benchmark.{partition}.{compiler}"
                              f".{branch_short}.num_node-{num_node}"
                              f".num_io_task-{num_io_task}"
                              f".{pio_role_scheme}"
                              f".{output_format}")
        config['num_node'] = num_node
        config['num_io_task'] = num_io_task

        run_script = (f"pio_benchmark.num_node-{num_node}"
                      f".num_io_task-{num_io_task}"
                      f".{pio_role_scheme}.run"
                      f".{output_format}")
        with open(run_script, mode="w") as f:
            f.write(template.render(**config))
            f.flush()
        for _ in range(3):
            if job_id == 0:
                # first submit
                shell_process = run(['sbatch', run_script],
                                    stdout=PIPE)
            else:
                # job has been submitted
                shell_process = run(['sbatch',
                                     dependency+str(job_id), run_script],
                                    stdout=PIPE)

            if shell_process.returncode == 0:
                # job submit successful
                job_id = shell_process.stdout.decode('UTF-8').rstrip()
                print(f"{run_script} submitted with ID {job_id}.")
            else:
                print(f"failed to submit {run_script}")

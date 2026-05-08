create database if not exists sentinela_db;
use sentinela_db;

create table if not exists logs_sensores (
	id int auto_increment primary key,
    nivel_gas int not null,
    distancia float not null,
    status_alarme boolean not null,
    data_hora timestamp default current_timestamp
);

create table if not exists logs_performance (
	id int auto_increment primary key,
    heap_livre int unsigned not null,
    tempo_task_sensores int unsigned,
    tempo_task_seguranca int unsigned,
    tempo_task_mysql int unsigned,
    tempo_task_web int unsigned,
    tempo_task_perf int unsigned,
    data_hora timestamp default current_timestamp
);
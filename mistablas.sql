DROP DATABASE IF EXISTS mistablas;
CREATE DATABASE mistablas;
USE mistablas;
CREATE TABLE jugadores(
	id_jugador INTEGER,
	user TEXT,
);
CREATE TABLE partidas(
	id_partidas INTEGER,
	id_jugadores INTEGER,
	puntuaciones INTEGER,
);
CREATE TABLE personajes(
	id_personaje INTEGER,
	nombre TEXT,
	id_atributos INTEGER,
	id_categoria INTEGER,
);
CREATE TABLE categorias(
	categoria TEXT,
	id_categoria INTEGER,
);
CREATE TABLE atributos(
	atributo TEXT,
	id_atributos INTEGER,
);
CREATE TABLE preguntas(
	pregunta TEXT,
	id_pregunta INTEGER,
	id_partida INTEGER,
	id_jugador INTEGER,
	id_atributo INTEGER,
	id_true INTEGER,
	id_false INTEGER,
);

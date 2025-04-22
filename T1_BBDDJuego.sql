DROP DATABASE IF EXISTS T1_BBDDJuego;
CREATE DATABASE T1_BBDDJuego;

USE T1_BBDDJuego;

CREATE TABLE jugadores(
	idj INT PRIMARY KEY NOT NULL ,
	usuario VARCHAR(60) NOT NULL,
	contrasenya VARCHAR(10) NOT NULL,
	puntuacion INT
)ENGINE = InnoDB;

CREATE TABLE partidas(
	idp INT PRIMARY KEY NOT NULL ,
	numjugadores INT NOT NULL,
	ganador VARCHAR(60) NOT NULL,
	equipo VARCHAR(60) NOT NULL,
	duracion VARCHAR(60) NOT NULL,
	fecha	VARCHAR(60) NOT NULL
)ENGINE = InnoDB;

CREATE TABLE participacion( 
	idPart INT NOT NULL,
	idJugadores INT NOT NULL,
	FOREIGN KEY (idPart) REFERENCES partidas (idp),
	FOREIGN KEY (idJugadores) REFERENCES jugadores (idj)
)ENGINE = InnoDB;







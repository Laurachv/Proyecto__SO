DROP DATABASE IF EXISTS mistablas;
CREATE DATABASE mistablas;
USE mistablas;

-- Tabla de jugadores
CREATE TABLE jugadores(
    Usuario VARCHAR(25),
	ID_jugador VARCHAR(10),
	Contraseña VARCHAR(10)
);


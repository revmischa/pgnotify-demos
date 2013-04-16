DROP DATABASE person_tracker;
CREATE DATABASE person_tracker;
\c person_tracker;

CREATE EXTENSION postgis;

DROP TABLE IF EXISTS person;
CREATE TABLE person (
    id serial PRIMARY KEY,
    name TEXT,
    loc GEOGRAPHY(POINT, 4326)
);

--

CREATE OR REPLACE FUNCTION update_person_table() RETURNS TRIGGER AS $$
DECLARE
BEGIN

PERFORM pg_notify('person_updated', '{"id": ' 
    || CAST(NEW.id AS text) 
    || ', "loc": '
    || ST_AsGeoJSON(NEW.loc)
    || '}'
);
RETURN NEW;

END;
$$ LANGUAGE plpgsql;

--

DROP TRIGGER IF EXISTS person_update_notify ON person;
CREATE TRIGGER person_update_notify AFTER UPDATE OR INSERT
    ON person FOR EACH ROW EXECUTE PROCEDURE update_person_table();


--
-- create a new person record
DELETE FROM person;
INSERT INTO person (name,loc) VALUES ('Mischa', ST_GeographyFromText('POINT(-122.25874046835327 37.87556521891249)'));
UPDATE person SET loc=ST_GeographyFromText('POINT(-122.25923935922987 37.86823636452294)');


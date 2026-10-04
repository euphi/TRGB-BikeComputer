"""Bikes, and which bike computer rode on which bike when.

A device is a bike computer as the log service knows it (the name of its pull target,
e.g. "gravel" for TRGB-BC.local, "pendler" for TRGB-FL.local). A bike carries what
the analysis needs and the rider does not: its weight and its aerodynamics/rolling
resistance (the power estimate of bikelog.report), later its service intervals.

An assignment says "from this day on, this device rides on that bike". A session gets
the bike of the latest assignment of its device that started on or before its day --
so moving a bike computer to another bike is a new assignment, and the old rides keep
their bike. (Several bikes per bike computer, told apart by their sensors, can later
add assignments of their own; the resolution stays the same.)

Pure data and functions; the log service keeps them in bikes.json (bikelogservice.analysis).
"""

from __future__ import annotations

import dataclasses
import datetime
import json
import uuid
from dataclasses import asdict, dataclass, field
from pathlib import Path

from .report import Athlete

TYPES = ("Gravel", "Rennrad", "MTB", "Trekking/Pendler", "Sonstiges")


@dataclass
class Bike:
    name: str
    id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])
    type: str = "Gravel"
    #: the bike ready to ride (bottles, bags, lights) -- the rider comes from Athlete.rider_kg
    mass_kg: float | None = None
    cda: float | None = None
    crr: float | None = None
    notes: str = ""


@dataclass
class Assignment:
    device: str
    bike_id: str
    since: datetime.date

    def as_dict(self) -> dict:
        return {"device": self.device, "bike_id": self.bike_id, "since": self.since.isoformat()}


@dataclass
class Registry:
    bikes: list[Bike] = field(default_factory=list)
    assignments: list[Assignment] = field(default_factory=list)

    def bike(self, bike_id: str | None) -> Bike | None:
        return next((b for b in self.bikes if b.id == bike_id), None)

    def bike_for(self, device: str, day: datetime.date | None) -> Bike | None:
        """The bike ``device`` rode on at ``day`` (None: no assignment covers it). Without
        a day (clock never set) the device's first assignment counts."""
        mine = sorted((a for a in self.assignments if a.device == device), key=lambda a: a.since)
        if not mine:
            return None
        if day is None:
            return self.bike(mine[0].bike_id)
        current = None
        for a in mine:
            if a.since <= day:
                current = a
        return self.bike(current.bike_id) if current else None

    def as_dict(self) -> dict:
        return {"bikes": [asdict(b) for b in self.bikes],
                "assignments": [a.as_dict() for a in self.assignments]}

    @classmethod
    def from_dict(cls, data: dict) -> "Registry":
        return cls(
            bikes=[Bike(**b) for b in data.get("bikes", [])],
            assignments=[Assignment(a["device"], a["bike_id"], datetime.date.fromisoformat(a["since"]))
                         for a in data.get("assignments", [])])

    @classmethod
    def load(cls, path: Path) -> "Registry":
        return cls.from_dict(json.loads(path.read_text(encoding="utf-8"))) if path.exists() else cls()

    def save(self, path: Path) -> None:
        tmp = path.with_name(path.name + ".part")
        tmp.write_text(json.dumps(self.as_dict(), ensure_ascii=False, indent=2), encoding="utf-8")
        tmp.replace(path)


def effective_athlete(athlete: Athlete, bike: Bike | None) -> Athlete:
    """The rider on this bike, as the power estimate needs it: system mass = rider + bike
    when both are known (else the rider's mass_kg, the old "rider + bike" figure), CdA and
    Crr from the bike when it has them."""
    if bike is None:
        return athlete
    out = dataclasses.replace(athlete)
    if bike.mass_kg and athlete.rider_kg:
        out.mass_kg = round(athlete.rider_kg + bike.mass_kg, 1)
    if bike.cda:
        out.cda = bike.cda
    if bike.crr:
        out.crr = bike.crr
    return out

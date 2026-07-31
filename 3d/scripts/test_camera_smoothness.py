#!/usr/bin/env python3
"""Deterministic continuity checks for the fine camera look-at path."""

from __future__ import annotations

from math import atan2, hypot, pi

from generate_pose_vectors import (
    CAMERA_LOOK_IN_POLE,
    CAMERA_LOOK_INVERTED,
    CAMERA_ROLL_UPRIGHT,
    camera_aim_at_point,
    empty_camera,
)


TURN = 32768
HALF_TURN = TURN // 2


def wrapped_delta(left: float, right: float) -> float:
    return (left - right + HALF_TURN) % TURN - HALF_TURN


def true_angle(y: int, x: int) -> float:
    return atan2(y, x) * TURN / (2 * pi)


def solve_front_flyby(
    distance: int,
    velocity: int,
    *,
    elevation: int = 0,
) -> tuple[list[int], list[float]]:
    camera = empty_camera()
    emitted: list[int] = []
    exact: list[float] = []
    for x in range(-distance, distance + 1, velocity):
        camera, carry = camera_aim_at_point(
            camera,
            (x, elevation, -distance),
            CAMERA_ROLL_UPRIGHT,
        )
        if carry:
            raise AssertionError(
                f"fly-by unexpectedly failed at D={distance}, x={x}"
            )
        if camera.euler[2] != 0:
            raise AssertionError("look-at introduced camera roll")
        if not elevation and camera.euler[0] != 0:
            raise AssertionError("level fly-by introduced camera pitch")
        if camera.look_flags & (CAMERA_LOOK_IN_POLE | CAMERA_LOOK_INVERTED):
            raise AssertionError("ordinary fly-by selected a pole branch")

        yaw_true = true_angle(-x, distance)
        yaw_error = abs(wrapped_delta(camera.euler[1], yaw_true))
        if yaw_error > 2:
            raise AssertionError(
                f"yaw error {yaw_error:.3f} raw at D={distance}, x={x}"
            )
        if elevation:
            pitch_true = true_angle(
                elevation, round(hypot(x, distance)),
            )
            pitch_error = abs(
                wrapped_delta(camera.euler[0], pitch_true)
            )
            if pitch_error > 3:
                raise AssertionError(
                    f"pitch error {pitch_error:.3f} raw"
                    f" at D={distance}, x={x}"
                )

        emitted.append(camera.euler[1])
        exact.append(yaw_true)

    for index in range(1, len(emitted)):
        actual_step = wrapped_delta(emitted[index], emitted[index - 1])
        exact_step = wrapped_delta(exact[index], exact[index - 1])
        if actual_step >= 0:
            raise AssertionError(
                f"non-monotonic yaw at D={distance}, sample={index}:"
                f" {actual_step}"
            )
        if abs(actual_step - exact_step) > 2:
            raise AssertionError(
                f"yaw step error at D={distance}, sample={index}:"
                f" actual={actual_step}, exact={exact_step:.3f}"
            )
        if abs(exact_step) >= 2 and actual_step == 0:
            raise AssertionError(
                f"fine camera froze at D={distance}, sample={index}"
            )
    return emitted, exact


def test_flybys() -> None:
    sequences: dict[tuple[int, int], list[int]] = {}
    for distance, velocity in (
        (256, 1),
        (1024, 1),
        (4096, 4),
        (16384, 16),
        (1024, 4),
        (1024, 16),
        (1024, 64),
    ):
        emitted, _ = solve_front_flyby(distance, velocity)
        sequences[distance, velocity] = emitted

    reference = sequences[1024, 1]
    if reference != sequences[4096, 4]:
        raise AssertionError("4x-scaled fly-by changed emitted yaw")
    if reference != sequences[16384, 16]:
        raise AssertionError("16x-scaled fly-by changed emitted yaw")

    for stride in (2, 3):
        sampled = reference[::stride]
        if any(left == right for left, right in zip(sampled, sampled[1:])):
            raise AssertionError(
                f"{stride}-tick render sampling retained a frozen yaw"
            )


def test_seam() -> None:
    camera = empty_camera()
    yaws: list[int] = []
    for x in range(-64, 65):
        camera, carry = camera_aim_at_point(
            camera, (x, 0, 1024), CAMERA_ROLL_UPRIGHT,
        )
        if carry:
            raise AssertionError("behind-camera seam solve failed")
        yaws.append(camera.euler[1])
    center = 64
    if yaws[center - 1:center + 2] != [16379, -16384, -16379]:
        raise AssertionError(
            f"negative-X seam changed: {yaws[center - 1:center + 2]}"
        )
    if any(
        wrapped_delta(right, left) <= 0
        for left, right in zip(yaws, yaws[1:])
    ):
        raise AssertionError("negative-X seam did not unwrap monotonically")


def test_elevated_and_near_pole() -> None:
    solve_front_flyby(1024, 4, elevation=512)

    camera = empty_camera(position=(0, -32768, 0))
    target = (1096, 1003, 30)
    camera, carry = camera_aim_at_point(
        camera, target, CAMERA_ROLL_UPRIGHT,
    )
    if carry or camera.look_flags & CAMERA_LOOK_IN_POLE:
        raise AssertionError("near-pole precision case entered the pole cone")
    yaw_true = true_angle(-1096, -30)
    error = abs(wrapped_delta(camera.euler[1], yaw_true))
    if error > 2:
        raise AssertionError(
            f"separate X/Z yaw normalization regressed: {error:.3f} raw"
        )


def main() -> None:
    test_flybys()
    test_seam()
    test_elevated_and_near_pole()
    print("PASS: deterministic fine-camera fly-by continuity")


if __name__ == "__main__":
    main()

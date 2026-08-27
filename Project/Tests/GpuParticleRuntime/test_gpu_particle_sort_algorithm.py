import math
import random
import unittest


INVALID_INDEX = 0xFFFFFFFF
SENTINEL = float("inf")


def sort_key(index: int, depths: dict[int, float]) -> tuple[float, int]:
    if index == INVALID_INDEX:
        return (SENTINEL, index)
    # HLSL uses negative NDC depth so ascending order becomes back-to-front.
    return (-depths[index], index)


def bitonic_sort(indices: list[int], depths: dict[int, float]) -> list[int]:
    result = list(indices)
    count = len(result)
    assert count > 0 and count & (count - 1) == 0

    level = 2
    while level <= count:
        mask = level >> 1
        while mask > 0:
            for index in range(count):
                partner = index ^ mask
                if partner <= index:
                    continue
                ascending = (index & level) == 0
                left_key = sort_key(result[index], depths)
                right_key = sort_key(result[partner], depths)
                should_swap = left_key > right_key if ascending else left_key < right_key
                if should_swap:
                    result[index], result[partner] = result[partner], result[index]
            mask >>= 1
        level <<= 1
    return result


class GpuParticleSortAlgorithmTests(unittest.TestCase):
    def test_bitonic_network_matches_back_to_front_reference_with_sentinels(self) -> None:
        # Small randomized networks reproduce the exact level/mask logic used by the HLSL before relying on 131072 slots.
        rng = random.Random(0xB17001C)
        for exponent in range(1, 9):
            capacity = 1 << exponent
            for _ in range(30):
                visible_count = rng.randint(0, capacity)
                depths = {index: rng.random() for index in range(visible_count)}
                indices = list(range(visible_count)) + [INVALID_INDEX] * (capacity - visible_count)
                rng.shuffle(indices)

                actual = bitonic_sort(indices, depths)
                expected_visible = sorted(range(visible_count), key=lambda index: (-depths[index], index))
                self.assertEqual(actual[:visible_count], expected_visible)
                self.assertTrue(all(index == INVALID_INDEX for index in actual[visible_count:]))

    def test_current_pool_has_expected_network_pass_count(self) -> None:
        levels = int(math.log2(131072))
        self.assertEqual(levels, 17)
        self.assertEqual(levels * (levels + 1) // 2, 153)


if __name__ == "__main__":
    unittest.main()

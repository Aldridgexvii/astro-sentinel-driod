import unittest

from python.astro_logic import parse_mood, sanitize_vlm_label


class LabelTests(unittest.TestCase):
    def test_strips_prefix_and_sentence(self):
        self.assertEqual(sanitize_vlm_label("Label: A blue ceramic mug on a desk."), "A blue ceramic mug on a desk")

    def test_unknown_for_empty_and_person_labels(self):
        self.assertEqual(sanitize_vlm_label(""), "unknown")
        self.assertEqual(sanitize_vlm_label("a person holding a mug"), "unknown")
        self.assertEqual(sanitize_vlm_label("a passport on a table"), "unknown")
        self.assertEqual(sanitize_vlm_label("unknown"), "unknown")

    def test_limits_word_count(self):
        label = sanitize_vlm_label("a small red ceramic coffee mug with a handle on a wooden table", max_words=6)
        self.assertLessEqual(len(label.split()), 6)


class MoodTests(unittest.TestCase):
    def test_parse_allowed_mood_from_wrapped_reply(self):
        self.assertEqual(parse_mood("My answer is CURIOUS"), "CURIOUS")

    def test_rejects_unknown_token(self):
        self.assertIsNone(parse_mood("HAPPYISH"))
        self.assertIsNone(parse_mood("ANGRY"))


if __name__ == "__main__":
    unittest.main()

<?php
// $Id$


/**
 * Implements hook_field_attach_form().
 */
function field_collection_field_attach_form($entity_type, $entity, &$form, &$form_state, $langcode) {
//  dpm($form);
  if (isset($form_state['collection_item']) && $entity_type != 'field_collection_item') {
    $collection_namespace = isset($form['#namespace']) ? $form['#namespace'] : 'default';

    foreach ($form_state['collection_item'] as $namespace => $item_info) {
      $collection_field_name = $item_info['collection_field_name'];

      $field = $form_state['field'][$collection_namespace][$collection_field_name][$langcode]['field'];
      $instance = $form_state['field'][$collection_namespace][$collection_field_name][$langcode]['instance'];
      $widget_type = $instance['widget']['type'];

      $form[$collection_field_name][$langcode]['#collection_name'] = $field['settings']['collection_name'];
      $form[$collection_field_name][$langcode]['#theme'] = $widget_type . '_form';
    }
  }
}

/**
 * Implements hook_field_attach_submit().
 */
function field_collection_field_attach_submit($entity_type, $entity, $form, &$form_state) {
  // Check if we have any 'field collection' fields in this entity
  if (isset($form_state['collection_item']) && $entity_type != 'field_collection_item') {
    // Get fields namespace of the parent entity
    $saved_namespace = isset($form['#namespace']) ? $form['#namespace'] : 'default';
//dpm($entity);
    $items = array();
    // Loop through 'field collection items'
    foreach ($form_state['collection_item'] as $namespace => $item_info) {
      $collection_item = $item_info['collection_item'];
      $collection_field_name = $item_info['collection_field_name'];
      $delta = $item_info['collection_delta'];

      $language = $form[$collection_field_name]['#language'];

      $form['#namespace'] = $namespace;
      field_attach_submit('field_collection_item', $collection_item, $form, $form_state);

      // If collection item is empty, then we must remove it from the list
      if (isset($collection_item->is_empty) && $collection_item->is_empty) {
        if (!empty($collection_item->item_id)) {
          field_collection_item_delete($collection_item->item_id);
        }
        unset($form_state['collection_item'][$namespace]);
        unset($form_state['field'][$namespace]);
      }
      // Or, just add it to the item list
      else {
        $entity->{$collection_field_name}[$language][$delta] = (array) $collection_item;
      }
//      if (isset($form_state['field_item_count'][$saved_namespace][$collection_field_name])) {
//        unset($form_state['field_item_count'][$saved_namespace][$collection_field_name]);
//      }
    }
//    dpm($entity);
  }
  // Check if collection item is empty, and if so, flag it for deletion
  elseif ($entity_type == 'field_collection_item') {
    $collection_item = (array) $entity;
    if (field_collection_item_is_empty($collection_item)) {
      $entity->is_empty = TRUE;
    }
  }
}

function field_collection_field_attach_update($entity_type, $entity) {
  // @todo Needs function body.
//  dpm($entity);
}

/**
 * Implements hook_field_delete().
 */
function field_collection_field_delete($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }
  }
}

/**
 * Implements hook_field_formatter_info().
 */
function field_collection_field_formatter_info() {
  return array(
    'field_collection' => array(
      'label' => t('Default'),
      'field types' => array('field_collection'),
    ),
  );
}

/**
 * Implements hook_field_info().
 */
function field_collection_field_info() {
  return array(
    'field_collection' => array(
      'label' => t('Field collection'),
      'description' => t('A field type which is a collection of other fields.'),
      'settings' => array(
        'collection_name' => '',
      ),
      'default_widget' => 'field_collection',
      'default_formatter' => 'field_collection',
    ),
  );
}

/**
 * Implements hook_field_insert().
 */
function field_collection_field_insert($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }
  }
}

/**
 * Implements hook_field_is_empty().
 */
function field_collection_field_is_empty($item, $field) {
  return field_collection_item_is_empty($item);
}

/**
 * Validates collections fields.
 *
 * @param mixed $form
 * @param mixed $form_state
 *
 * @see field_collection_field_widget_form().
 */
function field_collection_field_form_validate($form, &$form_state) {
  // Just set appropriate namespace in $form to validate fields of every
  // collection item in the form.
  $items = isset($form_state['collection_item']) ? $form_state['collection_item'] : array();
  foreach ($items as $namespace => $item_info) {
    $form['#namespace'] = $namespace;
    $collection_item = $item_info['collection_item'];
    // Avoid using entity_form_field_validate() here, because it incorrectly
    // identifies currently processing entity by copying full
    // $form_state['values'] structure.
    // Also, we can use previously saved field collection item entity, because
    // field_attach_form_validate() performs copying of values from $form_state
    // into provided object.
    field_attach_form_validate('field_collection_item', $collection_item, $form, $form_state);
  }
}

/**
 * Implements hook_field_load().
 */
function field_collection_field_load($entity_type, $entities, $field, $instances, $langcode, &$items, $age) {
  // Perform check on $entity_type to prevent infinite recursion
  if ($entity_type != 'field_collection_item' && $field['type'] == 'field_collection') {
    $collection_name = $field['settings']['collection_name'];
    // @todo Use field_collection_item_load_multiple() instead of this.
    foreach ($entities as $id => $entity) {
      foreach ($items[$id] as $delta => $item) {
        if (isset($item['item_id']) && $item['item_id'] > 0) {
          $collection_item = field_collection_item_load($item['item_id']);
          $collection_item = (array) $collection_item;
          $items[$id][$delta] += $collection_item;
        }
      }
    }
  }
}

/**
 * Implements hook_field_settings_form().
 */
function field_collection_field_settings_form($field, $instance, $has_data) {
  $settings = $field['settings'];
  $form = array();
  if ($field['type'] == 'field_collection') {
    $form['collection_name'] = array(
      '#type' => 'radios',
      '#title' => t('Field collection'),
      '#options' => field_collections_get_names(),
      '#default_value' => $settings['collection_name'],
      '#description' => t('Select field collection which you want to assign to this field.'),
      '#required' => TRUE,
      '#disabled' => $has_data,
    );
  }
  return $form;
}

/**
 * Implements hook_field_update().
 */
function field_collection_field_update($entity_type, $entity, $field, $instance, $langcode, &$items) {
  if ($field['type'] == 'field_collection') {
    // We must save 'field_collection_item' entities before parent
    // 'field_collection' field is saved, because we need to generate valid
    // {field_collection_item}.item_id's and update $items array with this
    // values.
    foreach ($items as $delta => $item) {
      $item = (object) $item;
      field_collection_item_save($item);
      $items[$delta] = (array) $item;
    }

    // Also if any of the items were removed we need to remove corresponding
    // field collection items.
//    db_delete('field_collection_item')->condition('tid', $term->tid)->execute();
  }
}

/**
 * Implements hook_field_widget_form().
 */
function field_collection_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
//  dpm($items);
//  dpm($form_state['values']);
  $widget_type = $instance['widget']['type'];
  $field_name = $field['field_name'];
  $collection_name = $field['settings']['collection_name'];

  // The 'item_id' property might not be initialized for new items, so we check
  // here by 'collection_name'.
  if (isset($items[$delta]['collection_name'])) {
    $collection_item = (object) $items[$delta];
  }
  // Initialize defaulted collection item
  else {
    $collection_item = field_collection_item_set_defaults(array('collection_name' => $collection_name));
  }
  // @todo Language must be handled in a different way. How?
  $collection_item->language = $langcode;

  // Prepare sub-form to append to the main $form
  $sub_form = array(
    '#collection_field_name' => $field_name,
    '#collection_name' => $collection_name,
    '#collection_delta' => $delta,
  );

  // Retrieve collection item form.
  // @todo Allow altering of this form via hook_form_alter() and friends.
  //   Maybe this is not neccesary because we have hook_field_attach_form().
  $sub_form = field_collection_item_form($sub_form, $form_state, $collection_item);

  $element += array(
    '#type' => 'fieldset',
    '#tree' => TRUE,
    '#field_name' => $field_name,
    '#delta' => $delta,
  );
  $element += $sub_form;

  // Add validate callback for form
  if (isset($form['#validate']) && ! in_array('field_collection_field_form_validate', $form['#validate'])) {
    $form['#validate'][] = 'field_collection_field_form_validate';
  }

  return $element;
}

/**
 * Implements hook_field_widget_info().
 */
function field_collection_field_widget_info() {
  return array(
//    'field_collection' => array(
//      'label' => t('Unformatted'),
//      'field types' => array('field_collection'),
//      'settings' => array(),
//      'behaviors' => array(
//        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
//        'default value' => FIELD_BEHAVIOR_DEFAULT,
//      ),
//    ),
    'field_collection_table' => array(
      'label' => t('Table'),
      'field types' => array('field_collection'),
      'settings' => array(),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_DEFAULT,
        'default value' => FIELD_BEHAVIOR_DEFAULT,
      ),
    ),
  );
}

/**
 * Returns HTML for an individual form element.
 *
 * Combine multiple values into a table with drag-n-drop reordering.
 * TODO : convert to a template.
 *
 * @param $variables
 *   An associative array containing:
 *   - element: A render element representing the form element.
 *
 * @ingroup themeable
 */
function theme_field_collection_table_form($variables) {
  $output = '';
  $element = $variables['element'];
  $multiple = ($element['#cardinality'] > 1 || $element['#cardinality'] == FIELD_CARDINALITY_UNLIMITED);

  $table_id = drupal_html_id($element['#collection_name'] . '_' . $element['#field_name'] . '_values');
  $order_class = $element['#collection_name'] . '-' . $element['#field_name'] . '-delta-order';
  $instances = field_info_instances('field_collection_item', $element['#collection_name']);

  // @todo Sort instances by weight

  // Prepare header
  $header = $multiple ? array('&nbsp;') : array();
  foreach ($instances as $key => $instance) {
    $header[] = $instance['label'];
  }
  if ($multiple) {
    $header[] = t('Order');
  }
//  dpm($element);
  // Sort items according to '_weight' (needed when the form comes back after
  // preview or failed validation)
  $items = array();
  foreach (element_children($element) as $key) {
    if ($key === 'add_more') {
      $add_more_button = &$element[$key];
    }
    else {
      $items[] = &$element[$key];
    }
  }
  usort($items, '_field_sort_items_value_helper');

  // Add the items as table rows.
  foreach ($items as $key => $item) {
    $cells = array();

    if ($multiple) {
      $item['_weight']['#attributes']['class'] = array($order_class);
      $delta_element = drupal_render($item['_weight']);
      $cells[] = array('data' => '', 'class' => array('field-multiple-drag'));
    }

    foreach ($instances as $field_name => $instance) {
      $cells[] = drupal_render($item[$field_name]);
    }

    if ($multiple) {
      $cells[] = array('data' => $delta_element, 'class' => array('delta-order'));
    }

    $rows[] = array(
      'data' => $cells,
      'class' => array('draggable'),
    );
  }

  $output = '<div class="form-item">';
  $output .= theme('table', array('header' => $header, 'rows' => $rows, 'attributes' => array('id' => $table_id, 'class' => array('field-multiple-table'))));
  $output .= $element['#description'] ? '<div class="description">' . $element['#description'] . '</div>' : '';
  if (isset($add_more_button)) {
    $output .= '<div class="clearfix">' . drupal_render($add_more_button) . '</div>';
  }
  $output .= '</div>';

  if ($multiple) {
    drupal_add_tabledrag($table_id, 'order', 'sibling', $order_class);
  }

  return $output;
}